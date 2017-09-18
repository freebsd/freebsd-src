#!/bin/sh

# This "verbose" ISIS protocol test involves a float calculation that
# may produce a slightly different result if the compiler is not GCC.
# Test only with GCC (similar to GitHub issue #333).

exitcode=0
test_name=isis-seg-fault-1-v

if [ ! -f ../Makefile ]
then
	printf '    %-35s: TEST SKIPPED (no Makefile)\n' $test_name
elif grep '^CC = .*gcc' ../Makefile >/dev/null
then
	passed=`cat .passed`
	failed=`cat .failed`
	if ./TESTonce $test_name isis-seg-fault-1.pcap isis-seg-fault-1-v.out '-v'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
else
	printf '    %-35s: TEST SKIPPED (compiler is not GCC)\n' $test_name
fi

exit $exitcode
