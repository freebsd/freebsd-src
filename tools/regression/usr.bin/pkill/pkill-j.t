#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..3"

name="pkill -j <jid>"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	jail / $base-1 127.0.0.1 $sleep 5 &
	chpid=$!
	jail / $base-2 127.0.0.1 $sleep 5 &
	chpid2=$!
	$sleep 5 &
	chpid3=$!
	sleep 0.5
	jid=`jls | awk "/127\\.0\\.0\\.1.*${base}-1/ {print \$1}"`
	if pkill -f -j $jid $sleep && sleep 0.5 &&
	    ! kill $chpid && kill $chpid2 $chpid3; then
		echo "ok 1 - $name"
	else
		echo "not ok 1 - $name"
	fi 2>/dev/null
	rm -f $sleep
else
	echo "ok 1 - $name # skip Test needs uid 0."
fi

name="pkill -j any"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	jail / $base-1 127.0.0.1 $sleep 5 &
	chpid=$!
	jail / $base-2 127.0.0.1 $sleep 5 &
	chpid2=$!
	$sleep 5 &
	chpid3=$!
	sleep 0.5
	if pkill -f -j any $sleep && sleep 0.5 &&
	    ! kill $chpid && ! kill $chpid2 && kill $chpid3; then
		echo "ok 2 - $name"
	else
		echo "not ok 2 - $name"
	fi 2>/dev/null
	rm -f $sleep
else
	echo "ok 2 - $name # skip Test needs uid 0."
fi

name="pkill -j none"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	$sleep 5 &
	chpid=$!
	jail / $base 127.0.0.1 $sleep 5 &
	chpid2=$!
	sleep 0.5
	if pkill -f -j none $sleep && sleep 0.5 &&
	    ! kill $chpid && kill $chpid2; then
		echo "ok 3 - $name"
	else
		echo "not ok 3 - $name"
	fi 2>/dev/null
	rm -f $sleep
else
	echo "ok 3 - $name # skip Test needs uid 0."
fi
