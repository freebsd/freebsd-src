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

$ Headers for printing errors/warnings.
$set 1

1 "Funktion:"

$ Error types.
$set 2

1 "Rechenfehler:"
2 "Analysefehler:"
3 "Laufzeitfehler:"
4 "Fataler Fehler:"
5 "Warnung:"

$ Math errors.
$set 3

1 "negative Zahl"
2 "Nicht-Ganzzahl-Wert"
3 "Überlauf: Zahl passt nicht in Register"
4 "Division durch 0"

$ Parse errors.
$set 4

1 "Ende der Datei"
2 "ungültiges Zeichen: '%c'"
3 "Zeichenketten-Ende konnte nicht gefunden werden"
4 "Kommentar-Ende konnte nicht gefunden werden"
5 "ungültiges Token"
6 "ungültiger Ausdruck"
7 "leerer Ausdruck"
8 "Ungültige Druck- oder Stream-Anweisung"
9 "Ungültige Funktionsdefinition"
10 "Ungültige Zuweisung: Die linke Seite muss \"scale\", \"ibase\", \"obase\", \"seed\", \"last\", \"var\" oder \"array element\" sein"
11 "keine automatische Variable gefunden"
12 "Funktionsparameter oder Variable \"%s%s\" existiert bereits"
13 "Blockende konnte nicht gefunden werden"
14 "eine \"void-Funktion\" kann keinen Wert zurückgeben: %s()"
15 "Variable kann keine Referenz sein: %s"
16 "POSIX erlaubt keine Namen mit mehr als 1 Zeichen Länge: %s"
17 "POSIX erlaubt keine '#'-Skriptkommentare"
18 "POSIX erlaubt das Schlüsselwort \"%s\" nicht"
19 "POSIX erlaubt keinen Punkt ('.') als Abkürzung für das letzte Ergebnis"
20 "POSIX benötigt Klammern um Rückgabeausdrücke"
21 "POSIX erlaubt den Operator \"%s\" nicht"
22 "POSIX erlaubt keine Vergleichsoperatoren außerhalb von if-Anweisungen oder Schleifen"
23 "POSIX benötigt 0 oder 1 Vergleichsoperatoren pro Bedingung"
24 "POSIX erlaubt keinen leeren Ausdruck in einer for-Schleife"
25 "POSIX verlangt einen Zeilenumbruch zwischen einem Semikolon und einer Funktionsdefinition"
26 "POSIX erlaubt keine exponentielle Notation"
27 "POSIX erlaubt keine Feld-Referenzen als Funktionsparameter"
28 "POSIX erlaubt keine ungültigen Funktionen"
29 "POSIX erfordert, dass die linke Klammer auf der gleichen Linie wie der Funktionskopf steht"
30 "POSIX erlaubt keine Zuweisung von Strings an Variablen oder Arrays"

$ Runtime errors.
$set 5

1 "ungültige \"ibase\": muss im Intervall [%lu, %lu] liegen"
2 "ungültige \"obase\": muss im Intervall [%lu, %lu] liegen"
3 "ungültige \"scale\"; muss im Intervall [%lu, %lu] liegen"
4 "ungültiger read()-Ausdruck"
5 "rekursiver read()-Aufruf"
6 "Variable oder Feld-Element hat den falschen Typ"
7 "Stapel hat zu wenig Elemente"
8 "Stapel für Register \"%s\" hat zu wenig Elemente"
9 "falsche Anzahl der Parameter: benötigt %zu, hat %zu"
10 "undefinierte Funktion: %s()"
11 "kann keinen ungültigen Wert in einem Ausdruck verwenden"

$ Fatal errors.
$set 6

1 "Speicherzuweisung fehlgeschlagen"
2 "Ein-Ausgabe-Fehler"
3 "konnte die Datei nicht öffnen: %s"
4 "Datei ist nicht Text: %s"
5 "Pfad ist ein Verzeichnis: %s"
6 "ungültige Befehlszeilenoption: \"%s\""
7 "Option erfordert ein Argument: '%c' (\"%s\")"
8 "Option benutzt keine Argumente: '%c' (\"%s\")"
9 "ungültiges Argument der Befehlszeilenoption: \"%s\""
