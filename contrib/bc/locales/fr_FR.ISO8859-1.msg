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

$ Error types.
$set 1

1 "Erreur de calcul :"
2 "Erreur d'analyse syntaxique :"
3 "Erreur d'exécution :"
4 "Erreur fatale :"
5 "Avertissement :"

$ Math errors.
$set 2

1 "nombre strictement négatif"
2 "nombre non entier"
3 "dépassement : le nombre ne tient pas dans un type traité par le processeur"
4 "division par 0"

$ Parse errors.
$set 3

1 "fin de fichier"
2 "caractère invalide '%c'"
3 "fin de chaîne non trouvée"
4 "fin de commentaire non trouvée"
5 "symbole invalide"
6 "expression invalide"
7 "expression vide"
8 "instruction d'écriture ou de flux invalide"
9 "définition de fonction invalide"
10 "affectation invalide : la partie gauche doit être 'scale', 'ibase', 'obase', 'seed', 'last', une variable ou une case de tableau"
11 "aucune variable auto trouvée"
12 "Le paramètre de fonction ou variable auto \"%s%s\" existe déjà"
13 "fin de bloc non trouvée"
14 "une fonction 'void' ne peut pas retourner de valeur : %s()"
15 "Une variable ne peut pas être une référence : %s"
16 "POSIX interdit les noms de plus d'un caractère : %s"
17 "POSIX interdit les commentaires dans les scripts (pas de '#')"
18 "POSIX interdit le mot-clé '%s'"
19 "POSIX interdit l'utilisation du point ('.') comme raccourci pour le dernier résultat"
20 "POSIX impose des parenthèses autour des expressions de retour"
21 "POSIX interdit l'opérateur '%s'"
22 "POSIX interdit les opérateurs de comparaison en dehors des expressions 'if' ou des boucles"
23 "POSIX impose 0 ou 1 opérateur de comparaison par condition"
24 "POSIX interdit une expression vide dans une boucle 'for'"
25 "POSIX exige une nouvelle ligne entre un point-virgule et une définition de fonction"
26 "POSIX interdit la notation exponentielle"
27 "POSIX interdit les références à un tableau dans les paramètres d'une fonction"
28 "POSIX n'autorise pas les fonctions void"
29 "POSIX impose que l'en-tête de la fonction et le '{' soient sur la même ligne"
30 "POSIX interdit pas d'assigner des chaînes de caractères à des variables ou à des tableaux"

$ Runtime errors.
$set 4

1 "ibase invalide : doit être [%lu, %lu]"
2 "obase invalide : doit être [%lu, %lu]"
3 "scale invalide : doit être [%lu, %lu]"
4 "expression read() invalide"
5 "appel read() récursif"
6 "mauvais type de variable ou d'élément de tableau"
7 "pile sous-remplie"
8 "pile pour le registre \"%s\" sous-remplie"
9 "nombre incorrect de paramètres - attendus : %zu, obtenus : %zu"
10 "fonction non définie : %s()"
11 "une valeur 'void' est inutilisable dans une expression"

$ Fatal errors.
$set 5

1 "échec d'allocation mémoire"
2 "erreur d'entrée-sortie"
3 "impossible d'ouvrir le fichier : %s"
4 "fichier non texte: %s"
5 "le chemin est un répertoire : %s"
6 "option de ligne de commande invalide : \"%s\""
7 "l'option '%c' (\"%s\") requiert un argument"
8 "l'option '%c' (\"%s\") ne prend pas d'argument"
9 "argument d'option de ligne de commande invalide : \"%s\""
