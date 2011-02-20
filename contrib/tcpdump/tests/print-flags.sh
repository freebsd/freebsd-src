#!/bin/sh

uudecode print-flags.puu

for i in x xx X XX A AA; do
	if (../tcpdump -$i -s0 -nr print-flags.pcap | tee NEW/print-$i.new | diff - print-$i.out >DIFF/print-$i.out.diff )
	then
		echo print-$i passed.
	else
		echo print-$i failed.
	fi
done
