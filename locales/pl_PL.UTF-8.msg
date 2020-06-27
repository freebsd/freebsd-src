$quote "

$ Różne wiadomości.
$set 1

1 "Funkcja:"

$ Typy błędów.
$set 2

1 "Błąd matematyczny:"
2 "Błąd parse'a:"
3 "Błąd biegu:"
4 "Błąd śmiertelny:"
5 "Ostrzeżenie:"

$ Błędy matematyczne.
$set 3

1 "liczba ujemna"
2 "numer nieintegracyjny"
3 "przelewanie: liczba nie mieści się w numerze sprzętowym"
4 "dzielenie przez 0"

$ Błędy Parse'a.
$set 4

1 "koniec akt"
2 "nieważny znak '%c'"
3 "koniec sznurka nie mógł być znaleziony"
4 "koniec komentarza nie mógł być znaleziony"
5 "nieważny token"
6 "nieważne wyrażenie"
7 "puste wyrażenie"
8 "nieważny wyciąg z wydruku"
9 "nieprawidłowa definicja funkcji"
10 "nieprawidłowe przyporządkowanie: lewa strona musi być elementem scale, ibase, obase, last, var lub element array"
11 "nie znaleziono zmiennej automatycznej"
12 "parametr funkcji lub auto \"%s%s\" już istnieje"
13 "końca bloku nie można było znaleźć"
14 "nie może zwrócić wartości z funkcji void: %s()"
15 "var nie może być odniesieniem: %s"
16 "POSIX nie zezwala na nazwy dłuższe niż 1 znak: %s"
17 "POSIX nie pozwala na komentarze skryptu '#'"
18 "POSIX nie pozwala na użycie następującego słowa kluczowego: %s"
19 "POSIX nie dopuszcza kropki ('.') jako skrótu do ostatniego wyniku"
20 "POSIX wymaga nawiasów wokół wyrażeń zwrotnych"
21 "POSIX nie pozwala następującemu operatorowi: %s"
22 "POSIX nie pozwala na porównywanie operatorów na zewnątrz, jeśli deklaracje lub pętle"
23 "POSIX wymaga 0 lub 1 operatora porównawczego na jeden warunek"
24 "POSIX wymaga, aby wszystkie 3 części pętli nie były puste"
25 "POSIX nie pozwala na notację wykładniczą"
26 "POSIX nie zezwala na odniesienia do tablicy jako parametrów funkcji"
27 "POSIX wymaga, aby lewe usztywnienie znajdowało się na tej samej linii co nagłówek funkcji"

$ Błędy Runtime'u.
$set 5

1 "nieprawidłowa ibase: musi być [%lu, %lu]"
2 "nieprawidłowa obase: musi być [%lu, %lu]"
3 "nieprawidłowa scale: musi być [%lu, %lu]"
4 "nieprawidłowe wyrażenie read()"
5 "powtarzalne wywołanie read()"
6 "element zmienny lub tablicowy jest niewłaściwym typem"
7 "stos ma zbyt mało elementów"
8 "niewłaściwa liczba parametrów; potrzeba %zu, mają %zu"
9 "niezdefiniowana funkcja: %s()"
10 "nie może użyć wartości pustej w wyrażeniu"

$ Fatalne błędy.
$set 6

1 "Alokacja pamięci nie powiodła się"
2 "Błąd we/wy"
3 "nie mógł otworzyć pliku: %s"
4 "plik nie jest ASCII: %s"
5 "ścieżka to katalog: %s"
6 "nieprawidłowa opcja wiersza poleceń: '%c' (\"%s\")"

$set 7

1 "opcja wymaga argumentu: '%c' (\"%s\")"
2 "opcja nie wymaga żadnych argumentów: '%c' (\"%s\")"
