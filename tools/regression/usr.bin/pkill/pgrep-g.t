#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pgrep-g.t,v 1.1 2005/03/20 12:38:08 pjd Exp $

base=`basename $0`

echo "1..2"

name="pgrep -g <pgrp>"
pgrp=`ps -o tpgid -p $$ | tail -1`
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -g $pgrp $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
kill $chpid
rm -f $sleep

name="pgrep -g 0"
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -g 0 $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
kill $chpid
rm -f $sleep
