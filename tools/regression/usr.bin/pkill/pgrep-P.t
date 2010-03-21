#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pgrep-P.t,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $

base=`basename $0`

echo "1..1"

name="pgrep -P <ppid>"
ppid=$$
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -P $ppid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill $chpid
rm -f $sleep
