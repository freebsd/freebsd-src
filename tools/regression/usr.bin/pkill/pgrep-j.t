#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

name="pgrep -j <jid>"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	jail / temp 127.0.0.1 $sleep 5 &
	sleep 0.3
	chpid=$!
	jid=`jls | egrep '127\.0\.0\.1.*temp.*\/' | awk '{print $1}'`
	pid=`pgrep -f -j $jid $sleep`
	if [ "$pid" = "$chpid" ]; then
		echo "ok 1 - $name"
	else
		echo "not ok 1 - $name"
	fi
	kill $chpid
	rm -f $sleep
else
	echo "ok 1 - $name # skip Test needs uid 0."
fi

name="pgrep -j 0"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	jail / temp 127.0.0.1 $sleep 5 &
	sleep 0.3
	chpid=$!
	pid=`pgrep -f -j 0 $sleep`
	if [ "$pid" = "$chpid" ]; then
		echo "ok 2 - $name"
	else
		echo "not ok 2 - $name"
	fi
	kill $chpid
	rm -f $sleep
else
	echo "ok 2 - $name # skip Test needs uid 0."
fi
