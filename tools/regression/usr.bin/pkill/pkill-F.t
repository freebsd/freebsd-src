#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

name="pkill -F <pidfile>"
pidfile=`mktemp /tmp/$base.XXXXXX` || exit 1
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
daemon -p $pidfile $sleep 5
sleep 0.3
pkill -f -F $pidfile $sleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
	;;
esac

rm -f $pidfile
rm -f $sleep
