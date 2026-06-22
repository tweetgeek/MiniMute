# MiniMute Companion App — Design

- **Data:** 2026-06-22
- **Projekt:** `software/` (aplikacja kliencka w monorepo MiniMute, obok `firmware/`)
- **Platforma:** macOS Apple Silicon (MVP). Intel mac i Windows — kolejne iteracje.
- **Stack:** Tauri v2 (Rust backend + webview), UI React + TypeScript + Vite, BLE przez `btleplug` (CoreBluetooth na macOS, WinRT na Windows w przyszłości).
- **Zakres MVP:** wykrywanie i połączenie z urządzeniami MiniMute, odbiór i wysyłanie komunikatów BLE, aplikacja w menubar działająca w tle, okno-konsola diagnostyczna. **Bez** logiki sterowania aplikacjami (Chrome/Teams) — architektura ją przewiduje (seam), ale nie budujemy jej teraz.

## 1. Cel i zakres

Aplikacja towarzysząca urządzeniu MiniMute (przycisk BLE do mute na spotkaniach). W MVP udowadnia dwukierunkową komunikację z urządzeniem i żyje w menubar:
- wykrywa urządzenia MiniMute (może być kilka), łączy się z nimi (parowanie obsługuje macOS),
- odbiera zdarzenia przycisku, status i poziom baterii,
- wysyła stan wskaźnika (Indicator) i komendy (Control: IDENTIFY, FORGET_BOND),
- udostępnia okno-konsolę diagnostyczną otwieraną z ikony w menubar.

Logika mapowania gestów na akcje w aplikacjach (mute Teams/Zoom/Meet/Chrome, kamera) jest **świadomie odłożona**; design zostawia czyste gniazdo (`ActionEngine`) do jej dołożenia.

Aplikacja jest też naturalnym **narzędziem do walidacji firmware'u** na sprzęcie (checklist sprzętowy firmware'u jest jeszcze nieodhaczony).

## 2. Architektura

Układ Tauri w `software/`: `src-tauri/` (Rust) + `src/` (React/Vite).

```
        ┌───────────────── Tray / Menubar (Tauri) ─────────────────┐
        │  ikona bez menu kontekstowego; klik → pokaż/ukryj okno    │
        │  aplikacja accessory (bez ikony w Docku)                  │
        └───────────────────────────┬───────────────────────────────┘
                                     │
   ┌──────────── Frontend: React/TS (webview, okno) ────────────┐
   │  DeviceList → DeviceCard, EventLog, ScanBar                  │
   │  store (Zustand) ← nasłuchuje zdarzeń Tauri, invoke komend   │
   └───────────────────────────┬─────────────────────────────────┘
              invoke (komendy)  │  emit (zdarzenia)
   ┌──────────── Backend: Rust (rdzeń, tokio async) ────────────┐
   │  commands   BleManager(btleplug)   DeviceRegistry           │
   │  protocol (mirror minimute_protocol.h)   events (domenowe)  │
   │  [seam] ActionEngine — PUSTY w MVP                          │
   └──────────────────────────────────────────────────────────────┘
```

**Moduły Rust (granice odpowiedzialności):**
- `ble` — `BleManager` za traitem `BleTransport`: skan, connect, subscribe, read/write charakterystyk; opakowuje btleplug. Trait pozwala testować resztę z atrapą.
- `protocol` — enkod/dekod zgodnie z `firmware/include/minimute_protocol.h` (kody przycisku, flagi indicator/status, komendy control, bateria, UUID-y). Lustrzane stałe + test pinujący wartości; nagłówek firmware'u to źródło prawdy.
- `devices` — `DeviceRegistry`: znane + połączone urządzenia, ich stan, trwałość na dysk (serde → katalog danych Tauri).
- `events` — domenowe zdarzenia emitowane do frontu.
- `commands` — handlery Tauri (`scan_start/stop`, `connect/disconnect/forget_device`, `send_indicator`, `send_control`, `list_devices`).

**Frontend (React/TS):** store (Zustand) nasłuchuje zdarzeń Tauri i utrzymuje listę urządzeń + log; komponenty `DeviceList`/`DeviceCard`/`EventLog`/`ScanBar`; komunikacja z backendem przez `invoke` (komendy) i `listen` (zdarzenia).

**Seam na przyszłość:** zdarzenia są semantyczne (urządzenie + gest), więc przyszły `ActionEngine` (mapowanie gest→akcja + automatyczne odsyłanie realnego stanu przez Indicator) tylko podłącza się do istniejącej magistrali zdarzeń i ścieżki wysyłania.

## 3. Kontrakt BLE (z firmware)

Serwis `MM_SERVICE_UUID` + charakterystyki: Button Event (Notify, urz→app), Indicator State (Write, app→urz), Device Status (Read+Notify, urz→app), Device Control (Write, app→urz) oraz standardowy Battery Service (Battery Level `0x2A19`, Notify). Definicje: `firmware/include/minimute_protocol.h`.

Rust `protocol` odwzorowuje ten nagłówek lustrzanie; test jednostkowy pinuje dokładne wartości bajtów (te same co testy `AppLinkProtocol` w firmware) — strzeże przed rozjechaniem kontraktu.

## 4. Cykl życia połączenia BLE

**Tożsamość urządzenia (macOS):** CoreBluetooth ukrywa MAC i nadaje stabilny per-host UUID peryferala → `device_id` (+ nazwa `MiniMute`). Zapisywany dla znanych urządzeń.

**Wykrywanie:** skan filtrowany po `MM_SERVICE_UUID` w advertising. Wiele urządzeń równolegle.

**Maszyna stanów per urządzenie:**
```
Discovered → Connecting → Connected(subscribed) → Disconnected
                                  ↑__________________|  (auto-reconnect gdy znowu w zasięgu)
flaga Known (utrwalona) niezależna od stanu połączenia
```

**Start aplikacji:** wczytaj znane → skan w tle → auto-connect znanych, gdy się pojawią → subskrybuj `Button`, `Status`, Battery `0x2A19` → odczytaj początkowy `Status` + `Battery`.

**Ręczny skan:** wykryte nieznane MiniMute → `ScanBar` → „Połącz" → connect + subscribe → oznacz Known + utrwal.

**Parowanie / szyfrowanie:** charakterystyki szyfrowane (`ENC_NO_MITM`). Na macOS pierwszy dostęp (subscribe/read/write) automatycznie wyzwala parowanie CoreBluetooth (Just Works, bez PIN); btleplug widzi sukces po zbondowaniu. To realizuje „połączenie robi aplikacja".

**Reconnect:** firmware przy bezczynności wpada w głęboki sen i rozłącza się; gdy się wybudzi i rozgłasza, skan w tle łączy ponownie. Na disconnect → stan `Disconnected`, Known zostaje.

**Sekwencja:** `Button` niesie `seq` — wykrywamy luki (log ostrzeżenia, np. po wybudzeniu).

**Obsługa błędów:** Bluetooth wyłączony / brak adaptera → globalny baner; brak uprawnienia macOS → komunikat z instrukcją; timeout/błąd GATT → stan błędu per urządzenie + retry z backoffem.

## 5. Okno i tray (menubar)

- **Ikona w menubar**, bez menu kontekstowego; lewy klik przełącza okno (przejmujemy zdarzenie kliknięcia).
- **Okno** zakotwiczone pod ikoną (prawy-górny róg); pokazanie + fokus na klik; **blur lub przycisk zamknięcia → ukrycie, nie wyjście**. Dopuszczalny start jako zwykłe okno, kotwiczenie do dopieszczenia.
- **Aplikacja accessory** (`ActivationPolicy::Accessory` / `LSUIElement`): brak ikony w Docku, brak górnego paska menu.
- **Wyjście:** kontrolka „Zakończ" w oknie (jedyny sposób, bo brak menu kontekstowego); `Cmd+Q` przy fokusie okna.
- **Autostart przy logowaniu:** plugin autostart Tauri; przełącznik „Uruchamiaj przy starcie" w oknie, domyślnie włączony.
- **Pierwsze uruchomienie:** okno pokazuje się raz na starcie, potem żyje w tray.

## 6. Model danych i UI konsoli (MVP)

**Model `Device` (Rust `DeviceRegistry`, serializowany do frontu):**
```
id: String            // UUID peryferala (CoreBluetooth)
name: String          // "MiniMute"
known: bool           // utrwalone
connection: Discovered | Connecting | Connected | Disconnected | Error(msg)
battery_percent: Option<u8>
status: { charging: bool, low_battery: bool, power_state: Active | ConnectedIdle }
last_button_event: Option<{ code: Short|Long|Double, seq: u8, at }>
indicator_sent: { muted, camera_off, in_call }   // ostatnio wysłany stan (widok aplikacji)
last_seen: timestamp
```

**Trwałość:** lista znanych urządzeń (`id, name, last_seen`) + ustawienia (autostart) → JSON w katalogu danych Tauri. Log zdarzeń w pamięci jako bufor pierścieniowy (np. ostatnie 500).

**Komponenty UI (React):**
- **Header:** tytuł, globalny wskaźnik Bluetooth (ok / wyłączony / brak uprawnień), przełącznik „Uruchamiaj przy starcie", przycisk „Zakończ".
- **ScanBar:** „Skanuj"; podczas skanu lista wykrytych nieznanych MiniMute z „Połącz".
- **DeviceList → DeviceCard** (per urządzenie): nazwa + kropka stanu (zielony/szary/czerwony); bateria % + ikona ładowania + niska bateria; tryb zasilania; ostatnie zdarzenie przycisku (kod + seq + czas); kontrolki: 3 przełączniki Indicator (Muted/Camera off/In call) → `send_indicator(flags)`, przyciski „Identify" (Control IDENTIFY) i „Forget bond" (Control FORGET_BOND), „Rozłącz"/„Zapomnij urządzenie".
- **EventLog:** przewijana lista ostatnich zdarzeń (czas, urządzenie, opis), odebrane vs wysłane — konsola testowa firmware'u.

**Przepływ:** notyfikacja BLE → `BleManager` dekoduje (`protocol`) → aktualizuje `DeviceRegistry` → emit zdarzenia Tauri → store React → render + wpis do logu. Klik „Identify" → `invoke send_control` → zapis charakterystyki Control.

## 7. Testy

- **Rust (`cargo test`):**
  - `protocol` — round-trip + pinowanie dokładnych bajtów (kody, flagi, maska/shift power, komendy, UUID-y); zgodność z testami `AppLinkProtocol` firmware.
  - `DeviceRegistry` — zdekodowane aktualizacje → poprawny stan (bateria, status, ostatnie zdarzenie, luka `seq`); round-trip serializacji.
  - `BleManager` za traitem `BleTransport` — orkiestracja z atrapą; realny btleplug walidowany ręcznie.
- **Frontend (Vitest + React Testing Library):** logika store'a (zdarzenia→stan), render `DeviceCard`/`EventLog`, kontrolki wysyłania wołają właściwą komendę (mock `invoke`).
- **E2E manualnie** z realnym MiniMute (przy okazji walidacja firmware'u). CI: `cargo test` + `vitest` bez sprzętu.

## 8. Pakowanie / uprawnienia macOS (MVP)

- Bundle Tauri `.app`, target Apple Silicon (aarch64).
- `Info.plist`: `NSBluetoothAlwaysUsageDescription`; `LSUIElement = true`.
- Plugin autostart.
- MVP: podpis ad-hoc / lokalny. Pełny podpis + notaryzacja odłożone (dystrybucja poza maszynę dev). Intel/universal i Windows (btleplug WinRT + tray Windows) — kolejne iteracje.

## 9. Seam na logikę akcji (poza MVP)

Magistrala zdarzeń semantycznych + ścieżka wysyłania Indicator to gniazda integracji. Przyszły `ActionEngine`: konfiguracja mapowania (urządzenie/gest → akcja w Teams/Zoom/Meet/Chrome), warstwa automatyzacji OS (macOS: Accessibility/AppleScript/hotkeys + wykrycie aktywnej aplikacji i realnego stanu mute), automatyczne odsyłanie realnego stanu przez Indicator (dziś ręczne). Granice MVP czynią to dodatkiem, nie przeróbką.

## 10. Ryzyka / otwarte kwestie

- **Asymetria bondu macOS** przy `FORGET_BOND`: urządzenie czyści swoją połówkę, host macOS trzyma swoją na poziomie systemu (GATT-peryferale słabo widoczne w UI Bluetooth) → reconnect może wymagać ręcznego odzysku. W MVP: jasny komunikat. Zsynchronizowany reset — iteracja.
- **Stabilność UUID peryferala** btleplug między restartami/resetami BT — zwykle stabilny per host; jeśli się zmieni, dopasowanie znanych pada → ponowne dodanie po nazwie.
- **Tauri v2:** lewy-klik tray = toggle + accessory app — zweryfikować dokładne API.
- **Finalne UUID-y** wciąż „robocze" w nagłówku firmware'u — zamrozić przed releasem.
- **Notaryzacja/podpis** odłożone — wymagane do dystrybucji.

## 11. Kryteria gotowości (MVP, Definition of Done)

1. Menubar accessory app (bez Docka); okno otwiera się klikiem, chowa na blur/zamknięcie; „Zakończ" z okna.
2. Skan + wykrycie po service UUID + połączenie (parowanie przez macOS) + auto-reconnect znanych na starcie.
3. Wiele urządzeń równolegle, każde jako karta (bateria, flagi, tryb, ostatnie zdarzenie).
4. Odbiór Button/Status/Battery na żywo; log zdarzeń aktualizowany.
5. Wysyłanie Indicator (przełączniki) i Control (IDENTIFY/FORGET_BOND); widoczne na urządzeniu.
6. Znane urządzenia + ustawienie autostartu utrwalone między uruchomieniami.
7. Testy Rust + frontu przechodzą; test `protocol` pinuje kontrakt; manualne e2e z urządzeniem.
