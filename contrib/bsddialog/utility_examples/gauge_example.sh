#!/bin/sh

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

