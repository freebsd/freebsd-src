#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

name="pkill -j <jid>"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	jail / temp 127.0.0.1 $sleep 5 &
	sleep 0.3
	jid=`jls | egrep '127\.0\.0\.1.*temp.*\/' | awk '{print $1}'`
	pkill -f -j $jid $sleep
	ec=$?
	case $ec in
	0)
		echo "ok 1 - $name"
		;;
	*)
		echo "not ok 1 - $name"
		;;
	esac
	rm -f $sleep
else
	echo "ok 1 - $name # skip Test needs uid 0."
fi

name="pkill -j 0"
if [ `id -u` -eq 0 ]; then
	sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
	ln -sf /bin/sleep $sleep
	jail / temp 127.0.0.1 $sleep 5 &
	sleep 0.3
	pkill -f -j 0 $sleep
	ec=$?
	case $ec in
	0)
		echo "ok 2 - $name"
		;;
	*)
		echo "not ok 2 - $name"
		;;
	esac
	rm -f $sleep
else
	echo "ok 1 - $name # skip Test needs uid 0."
fi
