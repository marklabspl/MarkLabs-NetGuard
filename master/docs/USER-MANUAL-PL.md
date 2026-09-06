# MarkLabs NetGuard — instrukcja obsługi

Dotyczy firmware `0.9.0-rc.6` dla WT32-ETH01 w trybie `PRODUCTION`.

## 1. Przeznaczenie

NetGuard monitoruje wskazane urządzenia i usługi sieciowe. Na podstawie utworzonych
reguł może wykonać RESET urządzenia podłączonego do jednego z czterech wyjść Power
Module. Cel diagnostyczny jest tylko obserwowany; urządzenie zasilane jest fizycznym
odbiornikiem przypisanym do CH1–CH4.

## 2. Pierwsze uruchomienie

1. Podłącz Ethernet i uruchom Master oraz Power Module.
2. Odczytaj przydzielony adres IP z DHCP routera albo z konsoli UART Mastera
   działającej z prędkością 115200 baud.
3. Otwórz `http://<adres-ip>/`.
4. Zaloguj się nazwą `admin` i początkowym hasłem `admin`.
5. W zakładce **Sieć** ustaw własne hasło. Hasło musi mieć 5–32 znaki.

Po pięciu nieudanych logowaniach panel blokuje kolejne próby na 60 sekund.

Konfiguracja fabryczna używa DHCP. Cele **Internet A** (`1.1.1.1`) i **Internet B**
(`8.8.8.8`) są aktywne jako testy PING. Urządzenia zasilane i reguły automatyzacji
są domyślnie wyłączone.

## 3. Ekran Status

Ekran główny pokazuje:

- adres IP, czas pracy, pamięć i przyczynę ostatniego resetu ESP32,
- stan, MAC, prędkość, duplex i liczbę zmian łącza Ethernet,
- stan Power Module, heartbeat, timeouty, błędne linie UART i stan komendy,
- wyniki celów, czas odpowiedzi, dostępność i liczbę sprawdzeń,
- urządzenia, aktywne reguły, postęp warunków i ostatnie decyzje,
- czas pozostały do końca stabilizacji i do następnej dozwolonej próby.

**Uruchom test** przyspiesza rozpoczęcie następnego cyklu. Nie przerywa testu, który
już trwa. **Wstrzymaj** zatrzymuje automatyczne akcje RESET, ale pomiary nadal działają.
**Ręczny RESET** uruchamia operację dla wybranego urządzenia; wymaga aktywnego
urządzenia, gotowego Power Module i co najmniej trzech kolejnych heartbeatów.

## 4. Ustawienia sieci i systemu

W zakładce **Sieć** można ustawić:

- **DHCP** — automatyczny adres; pola statyczne są wtedy zachowane, lecz nieużywane,
- **IPv4, maskę, bramę i DNS** — używane po wyłączeniu DHCP,
- **odstęp diagnostyczny** — 5–300 sekund, domyślnie 15 sekund,
- **automatykę** — globalne zezwolenie na wykonywanie reguł,
- **ochronę po uruchomieniu** — 0–3600 sekund, domyślnie 120 sekund,
- nowe hasło panelu — puste pole zachowuje aktualne hasło.

Zmiana DHCP lub statycznych parametrów sieci zaczyna działać po kliknięciu
**Restart / Uruchom ponownie**. Restart jest odrzucany podczas trwającego RESET.

## 5. Urządzenia podłączone do wyjść

W zakładce **Urządzenia i logika** dostępne są cztery urządzenia. Dla każdego ustaw:

- nazwę i ikonę,
- unikalne wyjście CH1–CH4,
- **RESET (1–300 s)** — czas wyłączenia zasilania,
- **Stabilizację (0–3600 s)** — czas po potwierdzonym RESET, w którym urządzenie
  uruchamia się i nie może być resetowane ponownie,
- **Ponowną próbę (0–86400 s)** — minimalny czas od potwierdzonego RESET do kolejnej
  operacji dla tego samego urządzenia,
- **Limit/h (0–20)** — maksymalną liczbę potwierdzonych RESET w godzinnym oknie;
  `0` wyłącza limit.

Dwa aktywne urządzenia nie mogą korzystać z tego samego wyjścia.

## 6. Cele diagnostyczne

Można utworzyć do 12 celów. Nazwa i adres są niezależne od urządzeń zasilanych.

- **PING** — rozwiązuje hostname, jeśli podano nazwę, i wysyła dwa pakiety ICMP.
- **TCP** — sprawdza możliwość otwarcia portu; port `0` oznacza port 80.
- **HTTP** — wysyła żądanie `HEAD` i wymaga ustawionego kodu odpowiedzi; port `0`
  oznacza 80.
- **HTTPS** — jak HTTP, domyślnie port 443. W 0.9.0-rc.6 certyfikat serwera nie jest
  weryfikowany, więc test potwierdza dostępność usługi, a nie jej tożsamość.
- **DNS** — sprawdza, czy podany hostname można rozwiązać na adres IP.

Timeout ma zakres 100–10000 ms. Dla HTTP/HTTPS ścieżka musi zaczynać się od `/`, a
oczekiwany status mieścić się w zakresie 100–599. Adres może być IPv4 albo hostname;
nie należy wpisywać `http://`, portu ani ścieżki w polu adresu.

## 7. Reguły automatyzacji

Można utworzyć do ośmiu reguł. Reguła zawiera 1–4 warunki, operator:

- **WSZYSTKIE (AND)** — każdy używany warunek musi być prawdziwy,
- **DOWOLNY (OR)** — wystarczy jeden prawdziwy warunek.

Warunek wskazuje cel i oczekiwany stan **ODPOWIADA** albo **NIE ODPOWIADA**. Pole
**Kolejne cykle (1–10)** określa, ile kolejnych wyników musi pasować przed próbą
RESET. Aktywna reguła musi wskazywać aktywny cel i aktywne urządzenie.

Przykład: `AP nie odpowiada` AND `router odpowiada` AND `Internet A odpowiada`
→ `RESET AP`. Taki układ ogranicza ryzyko restartu AP podczas awarii całej sieci.

## 8. Kolejność zabezpieczeń

Automatyczny RESET nastąpi dopiero, gdy warunki są spełnione przez wymaganą liczbę
cykli oraz:

- automatyka jest włączona i Ethernet ma adres IP,
- minęła ochrona po uruchomieniu,
- Power Module jest online i ma trzy kolejne poprawne heartbeat'y,
- zakończyła się stabilizacja i czas do ponownej próby,
- minął globalny odstęp 15 sekund od ostatniego potwierdzonego RESET,
- nie osiągnięto limitu godzinowego i nie trwa inna komenda.

Jednocześnie wykonywany jest najwyżej jeden RESET. Operacja jest uznana za udaną
dopiero po zdarzeniu UART potwierdzającym ponowne włączenie kanału. Ręczny RESET omija
wyłącznie warunki reguł; wszystkie ograniczenia czasowe i limit godzinowy nadal
obowiązują.

## 9. Zapisywanie konfiguracji

Przycisk **Zapisz całą logikę** zapisuje urządzenia, cele i reguły razem. Komunikat
powodzenia zawiera numer rewizji. Zapis jest odrzucany podczas RESET oraz gdy dane są
niepoprawne, np. dwa urządzenia używają tego samego CH albo reguła wskazuje wyłączony
cel. Nie opuszczaj strony przed pojawieniem się wyniku zapisu.

## 10. Kopia konfiguracji

Zakładka **Backup** eksportuje sieć, automatykę, urządzenia, cele i reguły do JSON.
Hasło panelu nie jest eksportowane. Import przyjmuje wyłącznie kopię produktu
`NETGUARD-MASTER` w formacie 1. Podczas importu automatyka jest najpierw wyłączana;
jeśli operacja się nie powiedzie, pozostaje wyłączona do ręcznej kontroli.

Po imporcie ustawień sieciowych wykonaj restart.

## 11. Home Assistant

W zakładce **Home Assistant** wpisz adres brokera MQTT, port, opcjonalne dane logowania,
nazwę urządzenia i prefixy. Domyślne prefixy to `homeassistant` i `marklabs/netguard`.
Po włączeniu integracji urządzenie oraz encje zostaną dodane przez MQTT Discovery.

Home Assistant otrzymuje sensory Ethernetu, Power Module, systemu i celów, przyciski
RESET oraz testu i przełącznik automatyki. Hasło MQTT pozostaje w NVS i nie trafia do
API odczytu ani backupu. Szczegółowa konfiguracja znajduje się w
`HOME-ASSISTANT-PL.md`.

## 12. Aktualizacja OTA

1. Pobierz obraz z końcówką `-app.bin` przeznaczony dla WT32-ETH01.
2. Otwórz **OTA**, wybierz plik i rozpocznij aktualizację.
3. Nie odłączaj zasilania ani Ethernetu do zakończenia zapisu.
4. Po komunikacie powodzenia urządzenie uruchomi się ponownie.

OTA jest odrzucane podczas RESET. Firmware sprawdza nazwę, nagłówek ESP32, rozmiar
i wynik zapisu, ale 0.9.0-rc.6 nie ma podpisu kryptograficznego ani automatycznego rollbacku.

## 13. Rozwiązywanie problemów

- **Panel się nie otwiera:** sprawdź link Ethernet, DHCP i adres z konsoli 115200.
- **Logowanie odrzucane:** odczekaj 60 sekund po serii błędnych prób.
- **Power Module offline:** sprawdź UART 9600 baud, RX/TX, izolator i heartbeat `PONG`.
- **Reguła nie wykonuje RESET:** odczytaj jej stan na ekranie głównym; może czekać na
  kolejne cykle, start, moduł, stabilizację, ponowną próbę, limit lub odstęp globalny.
- **Nie można zapisać:** przeczytaj kod błędu, usuń konflikt CH i sprawdź wszystkie
  aktywne cele oraz reguły.
- **Po zmianie IP brak panelu:** uruchom urządzenie ponownie i użyj nowego adresu.
- **MQTT rozłączony:** sprawdź adres, port, dane konta, tryb TLS i dostęp brokera;
  kod stanu jest dostępny przez endpoint statusu oraz komendę konsoli `MQTT`.

Konsola Mastera działa z prędkością 115200 baud. UART Power Module działa z
prędkością 9600 baud na GPIO32 (RX) i GPIO33 (TX). Master 0.9.0-rc.6 używa komendy
`RESET <SESJA> <ID> <CH> <czas>` protokołu NETGUARD-POWER 0.3.1. Wgraj najpierw Power 0.3.1, a następnie Master 0.9.0-rc.6.
