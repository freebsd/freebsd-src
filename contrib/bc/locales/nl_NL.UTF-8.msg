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

$ Diversen berichten.
$set 1

1 "Functie:"

$ Fouttypes.
$set 2

1 "Rekenfout:"
2 "Parse error:"
3 "Runtime error:"
4 "Fatale fout:"
5 "Waarschuwing:"

$ Math error.
$set 3

1 "negatief getal"
2 "niet-integraal getal"
3 "overloop: nummer past niet in een hardware-nummer"
4 "delen door 0"

$ Parsefouten.
$set 4

1 "einde van het file"
2 "ongeldig teken '%c'"
3 "string einde kon niet worden gevonden"
4 "commentaar einde kon niet worden gevonden"
5 "ongeldige token"
6 "ongeldige uitdrukking"
7 "lege uitdrukking"
8 "ongeldige print- of stream-instructie"
9 "ongeldige functiedefinitie"
10 "ongeldige toewijzing: linkerzijde moet scale, ibase, obase, last, var of array element zijn"
11 "geen autovariabele gevonden"
12 "Functieparameter of automatisch bestaat al"
13 "blokuiteinde kon niet worden gevonden"
14 "kan geen waarde uit de nietige functie teruggeven: %s()"
15 "var kan geen referentie zijn: %s"
16 "POSIX staat geen namen toe die langer zijn dan 1 teken: %s"
17 "POSIX staat geen '#'-scriptcommentaar toe"
18 "POSIX laat het volgende sleutelwoord niet toe: %s"
19 "POSIX staat geen periode ('.') toe als een kortere weg voor het laatste resultaat"
20 "POSIX vereist haakjes rond de terugkeeruitdrukkingen"
21 "POSIX laat de volgende operator niet toe: %s"
22 "POSIX laat geen vergelijking toe tussen operatoren buiten als verklaringen of lussen"
23 "POSIX vereist 0 of 1 vergelijkingsoperator per conditie"
24 "POSIX vereist dat alle 3 de delen van een lus niet leeg zijn"
25 "POSIX vereist een nieuwe regel tussen een puntkomma en een functiedefinitie"
26 "POSIX laat geen exponentiële notatie toe"
27 "POSIX staat geen arrayreferenties toe als functieparameters"
28 "POSIX staat geen lege functies toe"
29 "POSIX vereist dat de linkse beugel op dezelfde regel staat als de functiehoofding"
30 "POSIX staat niet toe dat strings worden toegewezen aan variabelen of arrays"

$ Runtime fouten.
$set 5

1 "ongeldige ibase: moet [%lu, %lu] zijn"
2 "ongeldige obase: moet [%lu, %lu] zijn"
3 "ongeldige schaal: moet [%lu, %lu] zijn"
4 "ongeldige read() expressie"
5 "recursieve read() call"
6 "Variabele of matrix-element is het verkeerde type"
7 "Stapel heeft te weinig elementen"
8 "Stapel voor register %s heeft te weinig elementen"
9 "Verkeerd aantal parameters; hebben %zu nodig, hebben %zu"
10 "ongedefinieerde functie: %s()"
11 "kan geen nietige waarde in een uitdrukking gebruiken"

$ Fatale fouten.
$set 6

1 "geheugentoewijzing mislukt"
2 "I/O-fout"
3 "kon geen file openen: %s"
4 "bestand is geen tekst: %s"
5 "pad is een directory: %s"
6 "ongeldige opdrachtregeloptie: %s"
7 "optie vereist een argument: '%c' (\"%s\")"
8 "optie neemt geen argumenten aan: '%c' (\"%s\")"
9 "ongeldige opdrachtregeloptie argument: %s"
