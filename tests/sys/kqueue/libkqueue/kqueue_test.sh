#!/bin/sh
# $FreeBSD$

skip=""
# Temporarily disable evfilt_proc tests: https://bugs.freebsd.org/233586
skip="${skip} --no-proc"
if [ "$(uname -p)" = "i386" ]; then
	# Temporarily disable timer tests on i386: https://bugs.freebsd.org/245768
	skip="${skip} --no-timer"
fi

i=1
"$(dirname $0)/kqtest" ${skip} | while read line; do
	echo $line | grep -q passed
	if [ $? -eq 0 ]; then
		echo "ok - $i $line"
		: $(( i += 1 ))
	fi

	echo $line | grep -q 'tests completed'
	if [ $? -eq 0 ]; then
		echo -n "1.."
		echo $line | cut -d' ' -f3
	fi
done
