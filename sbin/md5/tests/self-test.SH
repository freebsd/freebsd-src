#!/bin/sh

while read algorithm; do
	$algorithm -x > self-test.$algorithm.out || exitcode=$?
	diff %%TESTSBASE%%/sbin/md5/self-test.$algorithm.chk self-test.$algorithm.out
done < %%TESTSBASE%%/sbin/md5/algorithms.txt

exit $exitcode
