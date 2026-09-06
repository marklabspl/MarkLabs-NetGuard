# MarkLabs NetGuard — dokumentacja firmware

Kandydat do wydania: `0.9.0-rc.6`, sprzęt: WT32-ETH01, tryb: `PRODUCTION`.

## Architektura

Firmware ma jeden aktywny tor diagnostyki i automatyki. `LogicEngine` wykonuje testy
celów, oblicza reguły i jako jedyny może zlecić `RESET` modułowi zasilania.

## Panel WWW

- `/` — stan Ethernetu, UART, urządzeń, celów, reguł i ostatniej decyzji,
- `/config` — DHCP/statyczne IPv4, częstotliwość testów i hasło,
- `/logic` — cztery urządzenia zasilane, cele diagnostyczne i reguły,
- `/home-assistant` — broker MQTT, Discovery, TLS i stan połączenia,
- `/backup` — eksport i kontrolowane odtwarzanie kompletnej konfiguracji,
- `/update` — aktualizacja OTA plikiem `*-app.bin`.

Na ekranie głównym **Uruchom test** zleca nowy cykl bez czekania na interwał.
**Wstrzymaj** blokuje nowe akcje RESET, ale diagnostyka nadal pracuje. Ponowne
użycie przycisku wznawia automatykę.

Karta aktywnego urządzenia zawiera **Ręczny RESET**. Akcja używa czasu RESET
ustawionego dla urządzenia i może zostać przyjęta tylko wtedy, gdy Power Module jest
online, potwierdził trzy kolejne heartbeat'y i nie obsługuje innej komendy. Ręczne
wykonanie omija wyłącznie warunki reguły; nadal respektuje ochronę startową,
stabilizację, odstęp globalny, ponowną próbę i limit godzinowy. Operacja kończy się
dopiero po zdarzeniu UART `CHANNEL_ON` i jest zapisywana w dzienniku.

Podczas trwającego RESET zapis urządzeń, celów i reguł jest chwilowo blokowany. Chroni
to przed zmianą przypisania CH pomiędzy wysłaniem komendy a jej potwierdzeniem.

Login to `admin`. Dla nowej konfiguracji hasło początkowe to `admin`.

Status pokazuje również przyczynę ostatniego resetu ESP32, prędkość i duplex Ethernetu,
liczbę kolejnych poprawnych heartbeatów oraz informację, czy UART wykonuje komendę.
Ekran główny pokazuje także stan połączenia MQTT.

## Home Assistant

Opcjonalny moduł MQTT używa Discovery, retained state i Last Will. Publikuje stan
Ethernetu, Power Module, systemu, aktywnych celów oraz liczniki RESET. Udostępnia
przełącznik automatyki, przycisk testu i przyciski RESET aktywnych urządzeń. Komendy
RESET przechodzą przez `LogicEngine` i nie sterują wyjściami bezpośrednio.

Konfiguracja jest zapisywana w osobnej przestrzeni NVS z CRC. TLS szyfruje połączenie,
ale w tej wersji certyfikat brokera nie jest weryfikowany. MQTT jest domyślnie
wyłączone. Szczegóły zawiera `HOME-ASSISTANT-PL.md`.

## Model działania

Urządzenie zasilane oznacza fizyczny odbiornik przypisany do jednego wyjścia
CH1–CH4. Aktywnych urządzeń nie można przypisać do tego samego wyjścia.

Cel diagnostyczny jest niezależnym adresem obserwowanym przez PING, TCP, HTTP,
HTTPS albo DNS. Cel nie jest automatycznie powiązany z gniazdem. Jednego celu można
używać w wielu regułach.

Reguła łączy od jednego do czterech warunków operatorem WSZYSTKIE (AND) albo
DOWOLNY (OR). Dopiero po wymaganej liczbie kolejnych zgodnych cykli reguła może
zlecić RESET wskazanego urządzenia.

## Zabezpieczenia wykonania

RESET jest blokowany, dopóki:

- Ethernet nie ma aktywnego adresu IP,
- nie minął skonfigurowany czas ochronny po starcie (domyślnie 2 minuty),
- Power Module nie potwierdził co najmniej trzech kolejnych heartbeatów,
- trwa stabilizacja urządzenia,
- trwa odstęp do ponownej próby albo osiągnięto limit godzinowy,
- nie minął globalny odstęp pomiędzy restartami.

Zmiana modelu w czasie pomiaru unieważnia cały cykl. Liczniki kolejnych trafień są
zerowane po zmianie konfiguracji i po utracie sieci, więc stary wynik nie może
uruchomić nowej reguły. Sukces RESET jest liczony dopiero po potwierdzeniu UART.

Panel pokazuje osobny stan każdej reguły: czuwanie, odliczanie kolejnych cykli,
opóźnienie startowe, brak modułu zasilania, stabilizację, globalny odstęp, odstęp do ponownej próby,
limit godzinowy, zajęty moduł albo wysłany RESET. Dla urządzenia widoczne są pozostałe
sekundy stabilizacji i odstęp do ponownej próbyu oraz liczba potwierdzonych restartów w bieżącej
godzinie. Główny dziennik łączy 16 ostatnich zdarzeń automatyki z 24 zdarzeniami
systemowymi bieżącej sesji: startem, zmianami Ethernetu, Power Module, automatyki i OTA.

Globalny przełącznik automatyki i czas ochronny są również dostępne w ustawieniach
systemu. Zakres ochrony wynosi 0–3600 sekund. Wartość `0` służy wyłącznie do
kontrolowanych testów; w normalnej pracy zalecane jest co najmniej 120 sekund.

## UART

Master pracuje z prędkością 9600 baud na GPIO32 (RX) i GPIO33 (TX). Nadzoruje
heartbeat, a `PING`/`PONG` pozostaje mechanizmem awaryjnym. Akcja ma postać
`RESET <sesja Mastera> <transakcja> <kanał 1-4> <sekundy 1-300>` i wymaga odpowiedzi z identycznymi identyfikatorami. Potwierdzenie `OK RESET` oznacza przyjęcie polecenia, a dopiero `EVENT CHANNEL_ON` kończy operację i rozpoczyna stabilizację.
Moduł raportuje swoją sesję, restarty i logiczne stany CH1–CH4.

## Aktualizacja ustawień

Zmiana celów i reguł działa bez restartu. Zmiana DHCP lub statycznego IPv4 zaczyna
obowiązywać po ponownym uruchomieniu urządzenia. Przycisk restartu jest dostępny
bezpośrednio pod formularzem ustawień. Konfiguracja systemowa i model
logiki są przechowywane niezależnie w dwóch naprzemiennych rekordach NVS z CRC.

## Kopia konfiguracji i OTA

Kopia JSON zawiera konfigurację sieci, Home Assistant i automatyki oraz wszystkie
urządzenia, cele i reguły. Ze względów bezpieczeństwa nie zawiera haseł panelu ani
MQTT. Import akceptuje wyłącznie
produkt `NETGUARD-MASTER`, format 1 i prawidłowe rozmiary modelu. Najpierw wyłącza
automatykę, następnie zapisuje i waliduje model, a dopiero na końcu odtwarza poprzedni
stan automatyki. Po błędzie automatyka pozostaje wyłączona do kontroli użytkownika.

OTA wymaga uwierzytelnienia i żądania z tego samego panelu, akceptuje plik `*-app.bin`,
sprawdza nagłówek obrazu ESP32, dostępne miejsce i wynik walidacji biblioteki Update.
W czasie zapisu wszystkie ręczne i automatyczne akcje RESET są blokowane. Ta wersja
nie ma jeszcze podpisu kryptograficznego obrazu ani automatycznego powrotu do
poprzedniej partycji.

## Diagnostyka zapisu

Udany zapis logiki zwraca pole JSON `sequence` i pokazuje numer rewizji w panelu.
Konsola 115200 raportuje `LOGIC SAVE OK SEQUENCE=<n>` albo `LOGIC SAVE FAIL <kod>`.
Ustawienia systemowe analogicznie raportują `SYSTEM CONFIG SAVE ...`. Sekwencja
rośnie dopiero po zapisaniu i ponownym odczytaniu poprawnego rekordu NVS.
