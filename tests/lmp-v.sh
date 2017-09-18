#!/bin/sh

# The "verbose" Link Management Protocol test involves a float calculation that
# may produce a slightly different result depending on the architecture and the
# compiler (see GitHub issue #333). The reference output was produced using a
# GCC build and must reproduce correctly on any other GCC build regardless of
# the architecture.

exitcode=0

# A Windows build may have no file named Makefile and also a version of grep
# that won't return an error when the file does not exist. Work around.
if [ ! -f ../Makefile ]
then
	printf '    %-35s: TEST SKIPPED (no Makefile)\n' 'lmp-v'
elif grep '^CC = .*gcc' ../Makefile >/dev/null
then
	passed=`cat .passed`
	failed=`cat .failed`
	if ./TESTonce lmp-v lmp.pcap lmp-v.out '-T lmp -v'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
else
	printf '    %-35s: TEST SKIPPED (compiler is not GCC)\n' 'lmp-v'
fi

exit $exitcode
