#!/bin/sh

base=`basename $0`

echo "1..4"

name="pkill -x"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -P $$ -x slee
if [ $? -ne 0 ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
pkill -P $$ -x sleep
if [ $? -eq 0 ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
rm -f $sleep

name="pkill -x -f"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -P $$ -x -f "$sleep "
if [ $? -ne 0 ]; then
	echo "ok 3 - $name"
else
	echo "not ok 3 - $name"
fi
pkill -P $$ -x -f "$sleep 5"
if [ $? -eq 0 ]; then
	echo "ok 4 - $name"
else
	echo "not ok 4 - $name"
fi
rm -f $sleep
