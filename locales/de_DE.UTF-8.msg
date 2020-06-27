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
8 "Ungültige Druckanweisung"
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
25 "POSIX erlaubt keine exponentielle Notation"
26 "POSIX erlaubt keine Feld-Referenzen als Funktionsparameter"
27 "POSIX erfordert, dass die linke Klammer auf der gleichen Linie wie der Funktionskopf steht"

$ Runtime errors.
$set 5

1 "ungültige \"ibase\": muss im Intervall [%lu, %lu] liegen"
2 "ungültige \"obase\": muss im Intervall [%lu, %lu] liegen"
3 "ungültige \"scale\"; muss im Intervall [%lu, %lu] liegen"
4 "ungültiger read()-Ausdruck"
5 "rekursiver read()-Aufruf"
6 "Variable oder Feld-Element hat den falschen Typ"
7 "Stapel hat zu wenig Elemente"
8 "falsche Anzahl der Parameter: benötigt %zu, hat %zu"
9 "undefinierte Funktion: %s()"
10 "kann keinen ungültigen Wert in einem Ausdruck verwenden"

$ Fatal errors.
$set 6

1 "Speicherzuweisung fehlgeschlagen"
2 "Ein-Ausgabe-Fehler"
3 "konnte die Datei nicht öffnen: %s"
4 "Datei ist nicht ASCII: %s"
5 "Pfad ist ein Verzeichnis: %s"
6 "ungültige Befehlszeilenoption: \"%s\""
7 "Option erfordert ein Argument: '%c' (\"%s\")"
8 "Option benutzt keine Argumente: '%c' (\"%s\")"
