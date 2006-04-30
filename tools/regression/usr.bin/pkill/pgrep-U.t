#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

name="pgrep -U <uid>"
ruid=`id -ur`
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -U $ruid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
kill $chpid
rm -f $sleep

name="pgrep -U <user>"
ruid=`id -urn`
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -U $ruid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
kill $chpid
rm -f $sleep
