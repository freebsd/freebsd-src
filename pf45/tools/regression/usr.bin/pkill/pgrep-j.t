#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..3"

name="pgrep -j <jid>"
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
	pid=`pgrep -f -j $jid $sleep`
	if [ "$pid" = "$chpid" ]; then
		echo "ok 1 - $name"
	else
		echo "not ok 1 - $name"
	fi
	kill $chpid $chpid2 $chpid3
	rm -f $sleep
else
	echo "ok 1 - $name # skip Test needs uid 0."
fi

name="pgrep -j any"
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
	pids=`pgrep -f -j any $sleep | sort`
	refpids=`{ echo $chpid; echo $chpid2; } | sort`
	if [ "$pids" = "$refpids" ]; then
		echo "ok 2 - $name"
	else
		echo "not ok 2 - $name"
	fi
	kill $chpid $chpid2 $chpid3
	rm -f $sleep
else
	echo "ok 2 - $name # skip Test needs uid 0."
fi

name="pgrep -j none"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	$sleep 5 &
	chpid=$!
	jail / $base 127.0.0.1 $sleep 5 &
	chpid2=$!
	sleep 0.5
	pid=`pgrep -f -j none $sleep`
	if [ "$pid" = "$chpid" ]; then
		echo "ok 3 - $name"
	else
		echo "not ok 3 - $name"
	fi
	kill $chpid $chpid2
	rm -f $sleep
else
	echo "ok 3 - $name # skip Test needs uid 0."
fi
