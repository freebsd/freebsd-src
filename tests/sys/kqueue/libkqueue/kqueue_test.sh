#!/bin/sh
# $FreeBSD$

# Temporarily disable evfilt_proc tests: https://bugs.freebsd.org/233586
skip="--no-proc"

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
