#!/bin/sh
#
# @(#)dial.sh -- dialup remote using tip
#

#set -x

if [ $# -lt 1 ] ; then
	echo "$0: not enough arguments" 1>&2
	exit 1
fi

x=0

while ! tip $* && test $x -lt 3 
do
	sleep 5
	x=$(($x+1))
done

exit 0
