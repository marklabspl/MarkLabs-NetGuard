# Test sprzętowy NetGuard 0.9.0-rc.6

Procedura dotyczy klasycznego WT32-ETH01 z ESP32 i LAN8720.

1. Potwierdź komunikat `BOOT NETGUARD-MASTER 0.9.0-rc.6 PRODUCTION` na konsoli
   115200 baud.
2. Podłącz Ethernet i sprawdź `link`, `has_ip`, adres, MAC, prędkość oraz duplex na
   ekranie Status.
3. W trybie DHCP potwierdź przydzielenie adresu; następnie sprawdź konfigurację
   statyczną i powrót do DHCP.
4. Sprawdź dostęp do wszystkich zakładek, zmianę języka, logowanie oraz blokadę po
   pięciu błędnych próbach.
5. Połącz Power Module przez właściwy interfejs UART i potwierdź stan online po co
   najmniej trzech kolejnych heartbeat'ach.
6. Dla każdego CH1–CH4 wykonaj kontrolowany ręczny RESET i sprawdź dokładne
   potwierdzenie kanału oraz czasu.
7. Sprawdź PING, TCP, HTTP, HTTPS i DNS na znanych działających oraz niedziałających
   celach.
8. Sprawdź reguły ALL i ANY, kolejne cykle, ochronę startową, stabilizację, ponowną
   próbę, odstęp globalny i limit godzinowy.
9. Potwierdź, że jednocześnie wykonywany jest najwyżej jeden RESET i że utrata
   Ethernetu albo Power Module blokuje automatykę.
10. Zapisz pełny model, uruchom Master ponownie i sprawdź zachowanie ustawień.
11. Wykonaj eksport oraz import backupu i potwierdź, że hasło nie znajduje się w JSON.
12. Wgraj właściwy plik `-app.bin` przez OTA i potwierdź automatyczny restart.
13. Sprawdź MQTT Discovery, utratę i odzyskanie brokera, LWT, publikację stanów oraz
    komendy testu, automatyki i RESET z Home Assistant.
14. Wykonaj serię rozłączeń Ethernetu, dłuższy test heartbeatów oraz test stabilności
    co najmniej 24 godziny. Zapisz restarty, timeouty i błędne linie UART.

Test uznaje się za zakończony dopiero po wpisaniu wyników dla konkretnego egzemplarza
i potwierdzeniu wszystkich czterech kanałów.
