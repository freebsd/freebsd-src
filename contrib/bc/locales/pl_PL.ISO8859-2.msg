$ $
$ SPDX-License-Identifier: BSD-2-Clause
$ $
$ Copyright (c) 2018-2023 Gavin D. Howard and contributors.
$ $
$ Redistribution and use in source and binary forms, with or without
$ modification, are permitted provided that the following conditions are met:
$ $
$ * Redistributions of source code must retain the above copyright notice, this
$   list of conditions and the following disclaimer.
$ $
$ * Redistributions in binary form must reproduce the above copyright notice,
$   this list of conditions and the following disclaimer in the documentation
$   and/or other materials provided with the distribution.
$ $
$ THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
$ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
$ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
$ ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
$ LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
$ CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
$ SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
$ INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
$ CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
$ ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
$ POSSIBILITY OF SUCH DAMAGE.
$ $

$quote "

$ Typy b³êdów.
$set 1

1 "B³±d matematyczny:"
2 "B³±d parse'a:"
3 "B³±d biegu:"
4 "B³±d ¶miertelny:"
5 "Ostrze¿enie:"

$ B³êdy matematyczne.
$set 2

1 "liczba ujemna"
2 "numer nieintegracyjny"
3 "przelewanie: liczba nie mie¶ci siê w numerze sprzêtowym"
4 "dzielenie przez 0"

$ B³êdy Parse'a.
$set 3

1 "koniec akt"
2 "niewa¿ny znak '%c'"
3 "koniec sznurka nie móg³ byæ znaleziony"
4 "koniec komentarza nie móg³ byæ znaleziony"
5 "niewa¿ny token"
6 "niewa¿ne wyra¿enie"
7 "puste wyra¿enie"
8 "nieprawid³owe polecenie drukowania lub przesy³ania strumienia"
9 "nieprawid³owa definicja funkcji"
10 "nieprawid³owe przyporz±dkowanie: lewa strona musi byæ elementem scale, ibase, obase, last, var lub element array"
11 "nie znaleziono zmiennej automatycznej"
12 "parametr funkcji lub auto \"%s%s\" ju¿ istnieje"
13 "koñca bloku nie mo¿na by³o znale¼æ"
14 "nie mo¿e zwróciæ warto¶ci z funkcji void: %s()"
15 "var nie mo¿e byæ odniesieniem: %s"
16 "POSIX nie zezwala na nazwy d³u¿sze ni¿ 1 znak: %s"
17 "POSIX nie pozwala na komentarze skryptu '#'"
18 "POSIX nie pozwala na u¿ycie nastêpuj±cego s³owa kluczowego: %s"
19 "POSIX nie dopuszcza kropki ('.') jako skrótu do ostatniego wyniku"
20 "POSIX wymaga nawiasów wokó³ wyra¿eñ zwrotnych"
21 "POSIX nie pozwala nastêpuj±cemu operatorowi: %s"
22 "POSIX nie pozwala na porównywanie operatorów na zewn±trz, je¶li deklaracje lub pêtle"
23 "POSIX wymaga 0 lub 1 operatora porównawczego na jeden warunek"
24 "POSIX wymaga, aby wszystkie 3 czê¶ci pêtli nie by³y puste"
25 "POSIX wymaga nowej linii pomiêdzy ¶rednikiem a definicj± funkcji"
26 "POSIX nie pozwala na notacjê wyk³adnicz±"
27 "POSIX nie zezwala na odniesienia do tablicy jako parametrów funkcji"
28 "POSIX nie dopuszcza funkcji void"
29 "POSIX wymaga, aby lewe usztywnienie znajdowa³o siê na tej samej linii co nag³ówek funkcji"
30 "POSIX nie pozwala na przypisywanie ci±gów znaków do zmiennych lub tablic"

$ B³êdy Runtime'u.
$set 4

1 "nieprawid³owa ibase: musi byæ [%lu, %lu]"
2 "nieprawid³owa obase: musi byæ [%lu, %lu]"
3 "nieprawid³owa scale: musi byæ [%lu, %lu]"
4 "nieprawid³owe wyra¿enie read()"
5 "powtarzalne wywo³anie read()"
6 "element zmienny lub tablicowy jest niew³a¶ciwym typem"
7 "stos ma zbyt ma³o elementów"
8 "stos dla rejestru \"%s\" ma zbyt ma³o elementów"
9 "niew³a¶ciwa liczba parametrów; potrzeba %zu, maj± %zu"
10 "niezdefiniowana funkcja: %s()"
11 "nie mo¿e u¿yæ warto¶ci pustej w wyra¿eniu"

$ Fatalne b³êdy.
$set 5

1 "Alokacja pamiêci nie powiod³a siê"
2 "B³±d we/wy"
3 "nie móg³ otworzyæ pliku: %s"
4 "plik nie jest tekstem: %s"
5 "¶cie¿ka to katalog: %s"
6 "nieprawid³owa opcja wiersza poleceñ: \"%s\""
7 "opcja wymaga argumentu: '%c' (\"%s\")"
8 "opcja nie wymaga ¿adnych argumentów: '%c' (\"%s\")"
9 "nieprawid³owa argument opcja wiersza poleceñ: \"%s\""
