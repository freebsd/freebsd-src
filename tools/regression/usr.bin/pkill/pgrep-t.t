#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pgrep-t.t,v 1.1 2005/03/20 12:38:08 pjd Exp $

base=`basename $0`

echo "1..1"

name="pgrep -t <tty>"
tty=`ps -o tty -p $$ | tail -1`
if [ "$tty" = "??" ]; then
	tty="-"
fi
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -t $tty $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill $chpid
rm -f $sleep
