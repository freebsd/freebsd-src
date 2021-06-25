#!/bin/sh

/bin/cp %%TESTSBASE%%/sbin/md5/*.inp . || exit 127

exitcode=0

testloop () {
	opt=$1

	while read algorithm; do
		${algorithm}sum -c %%TESTSBASE%%/sbin/md5/${algorithm}.digest || exitcode=1
		${algorithm}sum -c %%TESTSBASE%%/sbin/md5/${algorithm}sum.digest || exitcode=1
	done < %%TESTSBASE%%/sbin/md5/algorithms.txt
}

testloop ""
testloop -q
testloop -r
testloop -qr

exit $exitcode
