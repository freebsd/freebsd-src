#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pkill-i.t,v 1.1 2005/03/20 12:38:08 pjd Exp $

base=`basename $0`

echo "1..1"

name="pkill -i"
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
usleep="${sleep}XXX"
touch $usleep
lsleep="${sleep}xxx"
ln -sf /bin/sleep $usleep
$usleep 5 &
sleep 0.3
pkill -f -i $lsleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
	;;
esac
rm -f $sleep $usleep
