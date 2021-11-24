#!/bin/sh

input="A B C D E F G H"
total=`echo $input | awk '{print split($0, a)}'`
curr=1
for i in $input
do
        perc="$(expr $(expr $curr "*" 100 ) "/" $total )"
        curr=`expr $curr + 1`
	./bsddialog --title " mixedgauge " --mixedgauge "Hello World! Press <ENTER>" 20 35 $perc \
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
	#sleep 1
done

