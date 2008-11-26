#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pkill-F.t,v 1.1.12.1 2008/10/02 02:57:24 kensmith Exp $

base=`basename $0`

echo "1..1"

name="pkill -F <pidfile>"
pidfile=`mktemp /tmp/$base.XXXXXX` || exit 1
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
echo $! > $pidfile
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
