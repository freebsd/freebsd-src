#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
# <http://creativecommons.org/publicdomain/zero/1.0/>.


input="A B C D E F G H"
total=`echo $input | awk '{print split($0, a)}'`
curr=1
for i in $input
do
        perc="$(expr $(expr $curr "*" 100 ) "/" $total )"
        curr=`expr $curr + 1`
	./bsddialog --title " mixedgauge " --mixedgauge "Example" 20 38 $perc \
		"Hidden!" 8	\
		"Label 1" 0	\
		"Label 2" 1	\
		"Label 3" 2	\
		"Label 4" 3	\
		"Label 5" 4	\
		"Label 6" 5	\
		"Label 7" 6	\
		"Label 8" 7	\
		"Label 9" 9	\
		"Label X" -- -$perc
	sleep 1
done

