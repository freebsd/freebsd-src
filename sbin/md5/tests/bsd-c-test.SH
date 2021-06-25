#!/bin/sh

exitcode=0

testloop () {
	opt=$1

	while read algorithm; do
		n=0
		for f in %%TESTSBASE%%/sbin/md5/*.inp; do
			n=$((n + 1))
			expected=$(head -$n %%TESTSBASE%%/sbin/md5/$algorithm.digest | tail -1 | cut -w -f4)
			hash=$($algorithm $opt -c "$expected" $f) || exitcode=1
		done
	done < %%TESTSBASE%%/sbin/md5/algorithms.txt
}

testloop ""
testloop -q
testloop -r
testloop -qr

exit $exitcode
