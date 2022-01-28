#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>.

./bsddialog --title " treeview " --treeview "Hello World!" 15 40 5 \
	0  "Tag 1"  "DESC 1 xyz"  off \
	1  "Tag 2"  "DESC 2 xyz"  off \
	2  "Tag 3"  "DESC 3 xyz"  on  \
	1  "Tag 4"  "DESC 4 xyz"  off \
	1  "Tag 5"  "DESC 5 xyz"  off \
	2>out.txt ; cat out.txt ; rm out.txt
