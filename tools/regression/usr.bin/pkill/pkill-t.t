#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

name="pkill -t <tty>"
tty=`ps -o tty -p $$ | tail -1`
if [ "$tty" = "??" ]; then
	tty="-"
fi
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -t $tty $sleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
	;;
esac
rm -f $sleep
