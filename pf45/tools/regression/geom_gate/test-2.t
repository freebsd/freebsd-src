#!/bin/sh
# $FreeBSD$

base=`basename $0`
us=45
work=`mktemp /tmp/$base.XXXXXX` || exit 1
src=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=$work bs=1m count=1 >/dev/null 2>&1
dd if=/dev/random of=$src bs=1m count=1 >/dev/null 2>&1
sum=`md5 -q $src`

ggatel create -u $us $work

dd if=${src} of=/dev/ggate${us} bs=1m count=1 >/dev/null 2>&1

echo '1..2'

if [ `md5 -q $work` != $sum ]; then
	echo 'not ok 1 - md5 checksum'
else
	echo 'ok 1 - md5 checksum'
	if [ `cat /dev/ggate${us} | md5 -q` != $sum ]; then
		echo 'not ok 2 - md5 checksum'
	else
		echo 'ok 2 - md5 checksum'
	fi
fi

ggatel destroy -u $us
rm -f $work $src
