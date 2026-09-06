# Model logiki NetGuard 0.9

System obsługuje cztery fizyczne urządzenia. Każde aktywne urządzenie ma unikalne
wyjście CH1–CH4, nazwę, ikonę, czas RESET, stabilizację, odstęp do ponownej próby
oraz opcjonalny limit operacji na godzinę.

Do 12 niezależnych celów może wykonywać PING, TCP, HTTP, HTTPS lub DNS. Cel nie jest
gniazdem zasilania i może być używany w wielu regułach.

Do ośmiu reguł łączy od jednego do czterech warunków przez WSZYSTKIE lub DOWOLNY.
Po wymaganej liczbie zgodnych cykli reguła może zlecić RESET urządzenia. Home
Assistant otrzymuje te same wyniki i może zlecić ręczny RESET przez zabezpieczoną
ścieżkę `LogicEngine`; nie steruje wyjściem CH bezpośrednio.
