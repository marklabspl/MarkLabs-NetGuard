# Integracja Home Assistant

Dotyczy NetGuard `0.9.0-rc.6`. Integracja używa MQTT Discovery i nie wymaga ręcznego
dodawania encji YAML.

## Wymagania

- działający broker MQTT dostępny z sieci NetGuard,
- integracja MQTT skonfigurowana w Home Assistant,
- osobne konto brokera dla NetGuard jest zalecane.

## Konfiguracja

1. Otwórz zakładkę **Home Assistant** w panelu NetGuard.
2. Wpisz adres brokera bez `mqtt://`, port, użytkownika i hasło.
3. Pozostaw prefix Discovery `homeassistant`, jeżeli nie został zmieniony w HA.
4. Ustaw prefix tematów, domyślnie `marklabs/netguard`.
5. Wybierz język nazw encji i wartości stanów: **Polski** albo **English**.
6. Włącz integrację i zapisz. Stan powinien przejść na **MQTT POŁĄCZONY**.

Port 1883 jest typowy dla połączenia bez TLS, a 8883 dla TLS. Tryb TLS w tej wersji
szyfruje transmisję, ale nie weryfikuje certyfikatu brokera. Do sieci niezaufanych
nie należy używać go bez dodatkowego zabezpieczenia sieci.

## Encje

Discovery tworzy jedno urządzenie rozpoznawane po MAC i udostępnia:

- Ethernet i Power Module jako sensory binarne,
- IP, uptime, wolną pamięć, cykle diagnostyczne oraz liczniki RESET,
- stan i czas odpowiedzi każdego aktywnego celu,
- stan każdego aktywnego urządzenia i reguły,
- wynik ostatniej komendy wysłanej z Home Assistant,
- przełącznik automatyki,
- przycisk uruchomienia testu,
- przycisk RESET każdego aktywnego urządzenia.

Przyciski RESET nie sterują CH bezpośrednio. Komenda przechodzi przez `LogicEngine`
i wymaga aktywnego urządzenia, gotowego Power Module, trzech heartbeatów oraz braku
innej operacji. Stan jest publikowany co 5 sekund i zachowywany przez broker.

NetGuard publikuje dostępność `online/offline` przez MQTT Last Will. Konfiguracje
Discovery są retained i są ponawiane po komunikacie `homeassistant/status = online`.

Hasła MQTT i panelu nie są eksportowane w backupie. Po imporcie na inne urządzenie
należy ponownie wpisać hasło MQTT.

W języku polskim stany urządzeń obejmują m.in. `GOTOWE`, `BRAK MODUŁU`,
`RESET W TOKU`, `STABILIZACJA`, `PONOWNA PRÓBA` i `LIMIT GODZINOWY`.
W języku angielskim odpowiadają im `READY`, `POWER OFFLINE`, `RESET PENDING`,
`STABILIZING`, `RETRY DELAY` i `HOURLY LIMIT`. Stan reguły również jest publikowany
w wybranym języku i wskazuje czuwanie, odliczanie cykli albo zabezpieczenie blokujące
RESET. Zmiana języka nie usuwa ustawień brokera; NetGuard odświeża wpisy Discovery.
