#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pkill-g.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

base=`basename $0`

echo "1..2"

name="pkill -g <pgrp>"
pgrp=`ps -o tpgid -p $$ | tail -1`
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -g $pgrp $sleep
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

name="pkill -g 0"
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -g 0 $sleep
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
