#!/bin/sh

base=`basename $0`

echo "1..1"

name="pgrep -n"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
oldpid=$!
$sleep 5 &
sleep 0.3
newpid=$!
pid=`pgrep -f -n $sleep`
if [ "$pid" = "$newpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill $oldpid
kill $newpid
rm -f $sleep
