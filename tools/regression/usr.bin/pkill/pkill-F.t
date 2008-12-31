#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pkill-F.t,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

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
