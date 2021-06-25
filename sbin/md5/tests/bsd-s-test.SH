#!/bin/sh

exitcode=0

testloop () {
	opt=$1
	sum=$2
	field=$3

	while read algorithm; do
		n=0
		for f in %%TESTSBASE%%/sbin/md5/*.inp; do
			n=$((n + 1))
			read text < $f
			hash=$($algorithm$sum $opt -s "$text" | cut -d= -f$field)
			hash=$(echo $hash | cut -w -f1)
			expected=$(head -$n %%TESTSBASE%%/sbin/md5/$algorithm.digest | tail -1 | cut -w -f4)
			[ "$hash" = "$expected" ] || exitcode=1
		done
	done < %%TESTSBASE%%/sbin/md5/algorithms.txt
}

testloop "" "" 2
testloop -q "" 1
testloop -r "" 1
testloop -qr "" 1
testloop "" sum 1
testloop -q sum 1

exit $exitcode
