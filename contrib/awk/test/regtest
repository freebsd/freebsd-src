#! /bin/sh

case "$AWK" in
"")	AWK=../gawk ;;
esac
#AWK=${AWK:-../gawk}

for i in reg/*.awk
do
	it=`basename $i .awk`
	$AWK -f $i <reg/$it.in >reg/$it.out 2>&1
	if cmp -s reg/$it.out reg/$it.good
	then
		rm -f reg/$it.out
	else
		echo "regtest: $it fails"
	fi
done
