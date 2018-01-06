#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
keyfile=`mktemp $base.XXXXXX` || exit 1
md=$(attach_md -t malloc -s `expr $sectors + 1`)

echo "1..3"

dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

geli init -B none -P -K $keyfile ${md}
geli attach -d -p -k $keyfile ${md}
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

rm -f $keyfile
