#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

name="pkill -U <uid>"
ruid=`id -ur`
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -U $ruid $sleep
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

name="pkill -U <user>"
ruid=`id -urn`
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -U $ruid $sleep
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
