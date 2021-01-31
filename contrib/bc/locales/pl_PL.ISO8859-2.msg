$ $
$ SPDX-License-Identifier: BSD-2-Clause
$ $
$ Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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

$ Ró¿ne wiadomo¶ci.
$set 1

1 "Funkcja:"

$ Typy b³êdów.
$set 2

1 "B³±d matematyczny:"
2 "B³±d parse'a:"
3 "B³±d biegu:"
4 "B³±d ¶miertelny:"
5 "Ostrze¿enie:"

$ B³êdy matematyczne.
$set 3

1 "liczba ujemna"
2 "numer nieintegracyjny"
3 "przelewanie: liczba nie mie¶ci siê w numerze sprzêtowym"
4 "dzielenie przez 0"

$ B³êdy Parse'a.
$set 4

1 "koniec akt"
2 "niewa¿ny znak '%c'"
3 "koniec sznurka nie móg³ byæ znaleziony"
4 "koniec komentarza nie móg³ byæ znaleziony"
5 "niewa¿ny token"
6 "niewa¿ne wyra¿enie"
7 "puste wyra¿enie"
8 "niewa¿ny wyci±g z wydruku"
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
25 "POSIX nie pozwala na notacjê wyk³adnicz±"
26 "POSIX nie zezwala na odniesienia do tablicy jako parametrów funkcji"
27 "POSIX wymaga, aby lewe usztywnienie znajdowa³o siê na tej samej linii co nag³ówek funkcji"

$ B³êdy Runtime'u.
$set 5

1 "nieprawid³owa ibase: musi byæ [%lu, %lu]"
2 "nieprawid³owa obase: musi byæ [%lu, %lu]"
3 "nieprawid³owa scale: musi byæ [%lu, %lu]"
4 "nieprawid³owe wyra¿enie read()"
5 "powtarzalne wywo³anie read()"
6 "element zmienny lub tablicowy jest niew³a¶ciwym typem"
7 "stos ma zbyt ma³o elementów"
8 "niew³a¶ciwa liczba parametrów; potrzeba %zu, maj± %zu"
9 "niezdefiniowana funkcja: %s()"
10 "nie mo¿e u¿yæ warto¶ci pustej w wyra¿eniu"

$ Fatalne b³êdy.
$set 6

1 "Alokacja pamiêci nie powiod³a siê"
2 "B³±d we/wy"
3 "nie móg³ otworzyæ pliku: %s"
4 "plik nie jest ASCII: %s"
5 "¶cie¿ka to katalog: %s"
6 "nieprawid³owa opcja wiersza poleceñ: '%c' (\"%s\")"

$set 7

1 "opcja wymaga argumentu: '%c' (\"%s\")"
2 "opcja nie wymaga ¿adnych argumentów: '%c' (\"%s\")"
