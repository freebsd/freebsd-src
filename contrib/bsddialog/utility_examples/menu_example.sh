#!/bin/sh

./bsddialog --title menu --menu "Hello World!" 15 30 5 \
	"Tag 1"	"DESC 1 xyz" \
	"Tag 2"	"DESC 2 xyz" \
	"Tag 3"	"DESC 3 xyz" \
	"Tag 4"	"DESC 4 xyz" \
	"Tag 5"	"DESC 5 xyz" \
	2>out.txt ; cat out.txt ; rm out.txt

./bsddialog --title checklist --checklist "Hello World!" 15 30 5 \
	"Tag 1"	"DESC 1 xyz" on  \
	"Tag 2"	"DESC 2 xyz" off \
	"Tag 3"	"DESC 3 xyz" on  \
	"Tag 4"	"DESC 4 xyz" off \
	"Tag 5"	"DESC 5 xyz" on  \
	2>out.txt ; cat out.txt ; rm out.txt

./bsddialog --title radiolist --radiolist "Hello World!" 15 30 5 \
	"Tag 1"	"DESC 1 xyz" off \
	"Tag 2"	"DESC 2 xyz" off \
	"Tag 3"	"DESC 3 xyz" on  \
	"Tag 4"	"DESC 4 xyz" off \
	"Tag 5"	"DESC 5 xyz" off \
	2>out.txt ; cat out.txt ; rm out.txt

./bsddialog --title buildlist --buildlist "Hello World!" 15 40 5 \
	"Tag 1"	"DESC 1 xyz" off \
	"Tag 2"	"DESC 2 xyz" off \
	"Tag 3"	"DESC 3 xyz" on  \
	"Tag 4"	"DESC 4 xyz" off \
	"Tag 5"	"DESC 5 xyz" off \
	2>out.txt ; cat out.txt ; rm out.txt

./bsddialog --title treeview --treeview "Hello World!" 15 40 5 \
	0 "Tag 1" "DESC 1 xyz" off \
	1 "Tag 2" "DESC 2 xyz" off \
	2 "Tag 3" "DESC 3 xyz" on  \
	1 "Tag 4" "DESC 4 xyz" off \
	1 "Tag 5" "DESC 5 xyz" off \
	2>out.txt ; cat out.txt ; rm out.txt
