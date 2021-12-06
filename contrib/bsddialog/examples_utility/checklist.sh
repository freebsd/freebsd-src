#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
# <http://creativecommons.org/publicdomain/zero/1.0/>.

./bsddialog --title checklist --checklist "Hello World!" 15 30 5 \
	"Tag 1"	"DESC 1 xyz" on  \
	"Tag 2"	"DESC 2 xyz" off \
	"Tag 3"	"DESC 3 xyz" on  \
	"Tag 4"	"DESC 4 xyz" off \
	"Tag 5"	"DESC 5 xyz" on  \
	2>out.txt ; cat out.txt ; rm out.txt
