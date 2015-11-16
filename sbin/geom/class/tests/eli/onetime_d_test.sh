#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
mdconfig -a -t malloc -s $sectors -u $no || exit 1

echo "1..3"

geli onetime -d md${no}
if [ -c /dev/md${no}.eli ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
# Be sure it doesn't detach on read.
dd if=/dev/md${no}.eli of=/dev/null 2>/dev/null
sleep 1
if [ -c /dev/md${no}.eli ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi
true > /dev/md${no}.eli
sleep 1
if [ ! -c /dev/md${no}.eli ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

mdconfig -d -u $no
