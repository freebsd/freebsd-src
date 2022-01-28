#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>.

perc=0
while [ $perc -le 100 ]
do
	./bsddialog --sleep 1 --title " mixedgauge "    \
		--mixedgauge "Example..." 20 45  $perc \
		"(Hidden)"   " -9"  \
		"Label  1"   " -1"  \
		"Label  2"   " -2"  \
		"Label  3"   " -3"  \
		"Label  4"   " -4"  \
		"Label  5"   " -5"  \
		"Label  6"   " -6"  \
		"Label  7"   " -7"  \
		"Label  8"   " -8"  \
		"Label  9"   " -10" \
		"Label 10"   " -11" \
		"Label  X"   $perc

	perc=`expr $perc + 20`
done
