#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

name="pgrep -t <tty>"
tty=`ps -o tty -p $$ | tail -1`
if [ "$tty" = "??" ]; then
	tty="-"
	ttyshort="-"
else
	ttyshort=`echo $tty | cut -c 4-`
fi
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -t $tty $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
pid=`pgrep -f -t $ttyshort $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
kill $chpid
rm -f $sleep
