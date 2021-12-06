#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
# <http://creativecommons.org/publicdomain/zero/1.0/>.

input="A B C D E F G"
total=`echo $input | awk '{print split($0, a)}'`
curr=1
for i in $input
do
	sleep 1
        perc="$(expr $(expr $curr "*" 100 ) "/" $total )"
        echo XXX
        echo $perc
        echo "[$curr/$total] Input: $i"
        echo XXX
        if [ $curr -eq $total ]
        then
                echo EOF
        fi
        curr=`expr $curr + 1`
done | ./bsddialog --title gauge --gauge "[0/$total] Starting..." 10 70 0

