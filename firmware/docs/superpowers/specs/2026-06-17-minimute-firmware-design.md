# MiniMute — Firmware Design

- **Data:** 2026-06-17
- **Urządzenie:** Seeed Studio XIAO nRF52840 (framework Adafruit nRF52 / Arduino, PlatformIO)
- **Status:** prototyp na płytce (docelowo własne PCB)
- **Zakres tego dokumentu:** firmware urządzenia. Aplikacja sterująca komputerem to osobny projekt — tutaj definiujemy też kontrakt BLE, po którym oba projekty się komunikują.

## 1. Cel i podział odpowiedzialności

MiniMute to przycisk BLE do sterowania spotkaniami: kliknięcie = mute/unmute mikrofonu, przytrzymanie = osobna akcja (np. kamera). Urządzenie działa na baterii, usypia się przy bezczynności, a stan sygnalizuje jedną diodą RGB.

**Urządzenie (ten projekt) — "głupi" kontroler:**
- Rozpoznaje gesty przycisku i wysyła generyczne zdarzenia (SHORT / LONG) przez BLE.
- Wystawia poziom baterii i status (ładowanie, niska bateria, tryb uśpienia).
- Odbiera od aplikacji stan wskaźnika (mute/live, kamera, czy trwa spotkanie) i odbija go na diodzie.
- Sam zarządza zasilaniem (dwustopniowy sen), parowaniem i bondem.

**Aplikacja (osobny projekt):**
- Mapuje zdarzenia na akcje (SHORT→mute, LONG→kamera, …) i wykonuje je w systemie.
- Po każdej zmianie odsyła do urządzenia aktualny stan wskaźnika.
- Czyta poziom baterii i status połączenia.

Mapowanie gestów na akcje żyje w aplikacji — zmiana mapowania nie wymaga przeflashowania urządzenia.

## 2. Architektura (modularny rdzeń + cienki HAL)

```
            ┌─────────────────────────────────────────────┐
  main.cpp  │  kompozycja + scheduler (millis) + callbacki  │
            └───────────────┬───────────────────────────────┘
                            │
   ┌─────────── Rdzeń logiki (testowalny natywnie) ───────────┐
   │  GestureRecognizer   PowerManager   LedStateResolver      │
   │  BatteryGauge        AppLinkProtocol (kodowanie zdarzeń)  │
   └───────────────┬──────────────────────────────────────────┘
                   │  interfejsy (porty)
   ┌─────────── HAL / adaptery nRF52 (na sprzęcie) ───────────┐
   │  Button   RgbLed   BatterySensor   BleLink (Bluefruit)    │
   └──────────────────────────────────────────────────────────┘
```

**Rdzeń logiki** — czyste klasy C++ bez `Arduino.h`, z wstrzykiwanym czasem (`Clock`) i portami (interfejsami). Testowalne natywnie:
- `GestureRecognizer` — z odbitych zmian stanu przycisku + czasu produkuje `ButtonEvent`.
- `LedStateResolver` — z wielu źródeł wylicza jeden docelowy `LedPattern` wg priorytetów.
- `PowerManager` — maszyna stanów zasilania, liczy timeouty i decyduje o przejściach.
- `BatteryGauge` — surowe mV → procent (krzywa LiPo) + wygładzanie + histereza progu.
- `AppLinkProtocol` — (de)serializacja zdarzeń/stanu zgodnie z kontraktem GATT.

**HAL / adaptery nRF52** — implementują porty: `NrfButton`, `NrfRgbLed`, `NrfBatterySensor`, `BleLink` (opakowanie Bluefruit: advertising, bond, serwisy, notyfikacje).

**Pętla:** główna pętla sterowana callbackami Bluefruit + lekki scheduler na `millis()`. Bez własnych tasków FreeRTOS (Bluefruit i tak używa go pod spodem).

## 3. Kontrakt BLE / GATT

Zapisany jako współdzielony nagłówek (`minimute_protocol.h`) — jedno źródło prawdy dla firmware'u i aplikacji (UUID-y, kody zdarzeń, bity stanu).

**Profil połączenia**
- Rola: BLE Peripheral. Nazwa rozgłaszania: `MiniMute`; w advertising m.in. UUID serwisu własnego.
- Parowanie: LESC, **Just Works** (brak wyświetlacza/klawiatury), łącze szyfrowane, bond zapisany we flash.
- Parametry połączenia: ACTIVE → krótki interwał (~15–30 ms); CONNECTED_IDLE → długi interwał + slave latency. Naciśnięcie przycisku może chwilowo poprosić o szybsze parametry.

**Serwisy standardowe**
- Device Information (0x180A): producent, model, wersja firmware/HW.
- Battery Service (0x180F): Battery Level (0x2A19), 0–100%, `Notify`.

**Serwis własny `MiniMute`** (UUID-y robocze, finalne wygenerujemy raz)
serwis `6E6D0001-B5A3-F393-E0A9-E50E24DCCA9E`

| Char | UUID (…0002/3/4/5) | Właściwości | Kierunek | Payload |
|---|---|---|---|---|
| **Button Event** | …0002 | Notify | urz→app | `[0]` typ: `0x01 SHORT`, `0x02 LONG`, `0x03 DOUBLE` (zarezerw.) · `[1]` seq (0–255) |
| **Indicator State** | …0003 | Write w/o resp | app→urz | `[0]` flagi: bit0 `muted`, bit1 `camera_off`, bit2 `in_call` |
| **Device Status** | …0004 | Read + Notify | urz→app | `[0]` flagi: bit0 `charging`, bit1 `low_battery`, bity2-3 `power_state` (0 active, 1 idle) |
| **Device Control** | …0005 | Write | app→urz | `[0]` cmd: `0x01 IDENTIFY`, `0x02 FORGET_BOND` |

**Uwagi:**
- `VERY_LONG` jest lokalne (re-parowanie) — nie ma go wśród zdarzeń wysyłanych do aplikacji; FORGET_BOND aplikacja może wywołać przez Control.
- `in_call` pozwala urządzeniu pokazywać kolor mute tylko wtedy, gdy trwa spotkanie.
- "Status komunikacji" w aplikacji wynika ze stanu połączenia BLE; `Device Status` dorzuca ładowanie + niską baterię + tryb uśpienia.
- `seq` przy zdarzeniach przycisku pozwala aplikacji wykryć zgubioną notyfikację.

## 4. Gesty przycisku

`GestureRecognizer` — wejście: odbity (debounce ~20 ms) stan przycisku + znaczniki czasu; wyjście: `ButtonEvent`.

| Gest | Warunek | Kiedy się wyzwala | Akcja |
|---|---|---|---|
| **SHORT** | naciśnięcie i puszczenie < `LONG_THRESH` | na puszczeniu (natychmiast) | wyślij SHORT → app |
| **LONG** | przytrzymanie ≥ `LONG_THRESH` (~600 ms) | w momencie przekroczenia progu | wyślij LONG → app |
| **VERY_LONG** | przytrzymanie ≥ `VERY_LONG_THRESH` (~3 s) | w momencie przekroczenia progu | lokalnie: zapomnij bond + tryb parowania |
| DOUBLE | — | — | zarezerwowane w protokole, wyłączone w v1 |

**Decyzja:** brak double-click w v1, by SHORT (mute) leciał natychmiast na puszczeniu (double-click wymagałby okna ~250–300 ms). Po przekroczeniu progu LONG puszczenie nie generuje już SHORT. Kod DOUBLE jest w kontrakcie na przyszłość.

## 5. Dioda RGB

`LedStateResolver` liczy jedną warstwę bazową + krótkie nakładki, bo mamy jedną diodę.

**Priorytet warstwy bazowej (od najwyższego):**
1. Parowanie / szukanie hosta (nie połączono).
2. Spotkanie aktywne (`in_call=1`) — stan mikrofonu.
3. Ładowanie (kabel, poza spotkaniem).
4. Bezczynny na baterii poza spotkaniem — zgaszona.

**Nakładki (krótkie, na wierzchu bazy):**
- Niska bateria — czerwony podwójny błysk co ~5 s.
- Potwierdzenie kliknięcia — pojedynczy jasny błysk ~80 ms.
- IDENTIFY — szybkie białe miganie ~3 s.

**Schemat kolorów (jasność ~20–30% przez PWM):**

| Stan | Kolor / wzorzec |
|---|---|
| Parowanie / szukanie | niebieski, wolne miganie (~1 Hz) |
| Spotkanie + MUTED | czerwony, ciągły |
| Spotkanie + LIVE | zielony, ciągły |
| Ładowanie (poza spotkaniem) | bursztynowy, oddychanie |
| Naładowano (kabel, poza spotkaniem) | zielony, ciągły, przygaszony |
| Niska bateria (nakładka) | czerwony podwójny błysk co ~5 s |
| Bezczynny na baterii / głęboki sen | zgaszona |

**Konflikt ładowanie vs spotkanie:** w spotkaniu stan mikrofonu wygrywa (ładowanie i tak widać w aplikacji).

**Driver:** `LedStateResolver` zwraca `LedPattern { color, mode: solid/blink/breathe, params }`; `NrfRgbLed` go renderuje (PWM; polaryzacja anoda/katoda ukryta w HAL — onboard XIAO jest active-LOW, docelowe PCB może być inne).

## 6. Zasilanie i sen

`PowerManager` — maszyna stanów:

| Stan | Co robi | Pobór |
|---|---|---|
| **ACTIVE** | niedawna aktywność; szybkie parametry BLE; normalne przetwarzanie | najwyższy |
| **CONNECTED_IDLE** (lekki sen) | połączony, brak aktywności ≥ `ACTIVE_TIMEOUT` (~30–60 s); długi interwał BLE + slave latency; CPU śpi między zdarzeniami; dioda zgaszona | niski |
| **DEEP_SLEEP** (System OFF) | rozłączony i bezczynny; ~µA; przycisk jako źródło wybudzenia (GPIO SENSE) | minimalny |

**Przejścia:**
- ACTIVE → CONNECTED_IDLE: połączony i brak aktywności ≥ `ACTIVE_TIMEOUT`.
- CONNECTED_IDLE → ACTIVE: naciśnięcie / zapis z aplikacji / zdarzenie połączenia.
- (gdy nie `in_call`) → DEEP_SLEEP: bezczynność ≥ `DEEP_SLEEP_TIMEOUT` (~30 min) lub rozłączony ≥ `DISCONNECTED_TIMEOUT` (~2–5 min). Przed wejściem: czyste rozłączenie BLE, zgaszenie diody, konfiguracja wybudzenia, `System OFF`.
- DEEP_SLEEP → (wybudzenie przyciskiem = reset nRF) → boot → ACTIVE + reconnect.

**Gwarancja:** `in_call=1` blokuje głęboki sen — podczas spotkania mute zawsze działa natychmiast.

**Wybudzenie z DEEP_SLEEP:** naciśnięcie wybudzające służy tylko do wybudzenia + reconnectu (reset + ~1–3 s łączenia) — nie jest wysyłane jako zdarzenie mute. Użytkownik klika ponownie po potwierdzeniu połączenia diodą.

Timery należą do `PowerManager` (czas wstrzykiwany) → testowalne.

## 7. Bateria i ładowanie

- Pomiar VBAT przez dzielnik (prototyp XIAO: enable `P0.14`=LOW, odczyt `P0.31`; docelowe PCB: własny dzielnik), okresowo (~co 30–60 s i przy wybudzeniu), nie ciągle.
- `BatteryGauge`: mV → % wg krzywej LiPo (lookup) + wygładzanie (EMA/mediana) + histereza progu niskiej baterii (np. załącz <10%, zwyłącz >15%).
- Ładowanie: pin STAT ładowarki (XIAO `P0.17`); "naładowano" = obecny VBUS i brak ładowania.
- Na zmianę: aktualizacja BAS (%) oraz flag `Device Status` (charging, low_battery) → Notify.

## 8. Parowanie / bond

- Pierwszy boot, brak bondu: rozgłaszanie + parowanie LESC Just Works → klucze we flash. Dioda: niebieskie miganie.
- Kolejne wybudzenia: jest bond → rozgłaszanie ukierunkowane na znanego hosta → szyfrowanie zapisanymi kluczami. Niebieskie miganie do połączenia.
- VERY_LONG / Control FORGET_BOND: kasuje bond, rozłącza, wraca do świeżego parowania.
- Utrata połączenia: restart rozgłaszania (reconnect); po `DISCONNECTED_TIMEOUT` → głęboki sen.

Bond do jednego komputera (model "single host").

## 9. Testy

Logikę testujemy natywnie na hoście (`[env:native]` + Unity; szybko, w CI):

| Moduł | Co weryfikujemy |
|---|---|
| `GestureRecognizer` | SHORT na puszczeniu, LONG na progu, VERY_LONG, brak SHORT po LONG, debounce |
| `LedStateResolver` | kombinacje wejść → poprawny `LedPattern` i kolejność priorytetów |
| `PowerManager` | przejścia stanów, `in_call` blokuje głęboki sen, timeouty |
| `BatteryGauge` | mapowanie %, wygładzanie, histereza progu |
| `AppLinkProtocol` | round-trip enkod/dekod — pilnuje zgodności z kontraktem GATT |

Wszystko z wstrzykiwanym `Clock` i portami — deterministyczne, bez `Arduino.h`.

**Walidacja na sprzęcie (manualna):** parowanie z Mac i Windows, reconnect po wybudzeniu, zdarzenia w skanerze (nRF Connect), wizualna weryfikacja stanów diody, odczyt %, wykrycie ładowania, pomiar poboru (deep sleep µA / idle / active) power profilerem.

## 10. Ryzyka / otwarte kwestie

- **Bluefruit + System OFF:** zweryfikować API bondu/whitelisty, bezpieczne zatrzymanie SoftDevice przed `System OFF`, realny czas reconnectu po wybudzeniu (reset).
- **Cel żywotności baterii + pojemność LiPo** docelowego PCB — do ustalenia (bez tego brak oszacowania dni pracy).
- **Just Works** (brak ochrony MITM) — zakładamy akceptowalne dla tego urządzenia.
- **Szczegóły docelowego PCB** (polaryzacja diody, dzielnik baterii, pin STAT ładowarki, detekcja VBUS) — HAL je abstrahuje, doprecyzujemy przy schemacie.
- **Finalne UUID-y** — wygenerować raz i zamrozić w `minimute_protocol.h`.

## 11. Kryteria gotowości prototypu (Definition of Done)

1. Paruje i bonduje z Mac i Windows; auto-reconnect po wybudzeniu; FORGET_BOND działa (przycisk i z aplikacji).
2. SHORT/LONG docierają do aplikacji (z `seq`); VERY_LONG lokalnie re-paruje.
3. Aplikacja czyta % baterii i flagi (charging, low_battery); dioda odbija mute/live z aplikacji.
4. Dioda realizuje pełny schemat kolorów z priorytetami.
5. Dwustopniowy sen działa, `in_call` blokuje głęboki sen, przycisk wybudza.
6. Testy natywne przechodzą; checklista sprzętowa odhaczona.
