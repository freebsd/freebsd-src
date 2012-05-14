#!/bin/sh

for i in x xx X XX A AA; do
	#
	# We cannot rely on, for example, "print-x.out" and
	# "print-X.out" being different files - we might be running
	# this on a case-insensitive file system, e.g. a Windows
	# file system or a case-insensitive HFS+ file system on
	# Mac OS X.
	#
	# Therefore, for "X" and "XX", we have "print-capX.out"
	# and "print-capXX.out".
	#
	if test $i = X
	then
		printname=capX
	elif test $i = XX
	then
		printname=capXX
	else
		printname=$i
	fi
	if (../tcpdump -$i -s0 -nr print-flags.pcap | tee NEW/print-$printname.new | diff - print-$printname.out >DIFF/print-$printname.out.diff )
	then
		echo print-$i passed.
	else
		echo print-$i failed.
	fi
done
