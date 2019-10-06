#!/bin/sh

# This "verbose" ISIS protocol test involves a float calculation that
# may produce a slightly different result if the compiler is not GCC.
# Test only with GCC (similar to GitHub issue #333).

exitcode=0
test_name=isis-seg-fault-1-v

srcdir=${1-..}
: echo $0 using ${srcdir}

testdir=${srcdir}/tests
passedfile=tests/.passed
failedfile=tests/.failed
passed=`cat ${passedfile}`
failed=`cat ${failedfile}`

if [ ! -f Makefile ]
then
	printf '    %-35s: TEST SKIPPED (no Makefile)\n' $test_name
elif grep '^CC = .*gcc' Makefile >/dev/null
then
	if ${testdir}/TESTonce $test_name ${testdir}/isis-seg-fault-1.pcap ${testdir}/isis-seg-fault-1-v.out '-v'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
else
	printf '    %-35s: TEST SKIPPED (compiler is not GCC)\n' $test_name
fi

exit $exitcode
