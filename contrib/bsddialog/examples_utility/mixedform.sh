#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>.

./bsddialog --insecure --title " mixedform " --mixedform "Hello World!" 12 40 5 \
	Label:    11  Entry      1  11  18  25  0 \
	Label:    21  Read-Only  2  11  18  25  2 \
	Password: 31  ""         3  11  18  25  1 \
	2>out.txt ; cat out.txt ; rm out.txt
