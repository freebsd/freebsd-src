#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
md=$(attach_md -t malloc -s $sectors)

echo "1..3"

geli onetime -d ${md}
if [ -c /dev/${md}.eli ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
# Be sure it doesn't detach on read.
dd if=/dev/${md}.eli of=/dev/null 2>/dev/null
sleep 1
if [ -c /dev/${md}.eli ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi
true > /dev/${md}.eli
sleep 1
if [ ! -c /dev/${md}.eli ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

mdconfig -d -u ${md}
