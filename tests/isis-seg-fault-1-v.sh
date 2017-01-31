#!/bin/sh

# This "verbose" ISIS protocol test involves a float calculation that
# may produce a slightly different result if the compiler is not GCC.
# Test only with GCC (similar to GitHub issue #333).

test_name=isis-seg-fault-1-v

if [ ! -f ../Makefile ]
then
	printf '    %-35s: TEST SKIPPED (no Makefile)\n' $test_name
elif grep '^CC = .*gcc' ../Makefile >/dev/null
then
  ./TESTonce $test_name isis-seg-fault-1.pcap isis-seg-fault-1-v.out '-t -v'
else
	printf '    %-35s: TEST SKIPPED (compiler is not GCC)\n' $test_name
fi
