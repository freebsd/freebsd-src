#!/bin/sh
# $FreeBSD$

base=`basename $0`
us=45
work="/dev/md${us}"
src="/dev/md`expr $us + 1`"

mdconfig -a -t malloc -s 1M -u $us || exit 1
mdconfig -a -t malloc -s 1M -u `expr $us + 1` || exit 1
dd if=/dev/random of=$work bs=1m count=1 >/dev/null 2>&1
dd if=/dev/random of=$src bs=1m count=1 >/dev/null 2>&1
sum=`cat $src | md5 -q`

ggatel create -u $us $work

dd if=${src} of=/dev/ggate${us} bs=1m count=1 >/dev/null 2>&1

if [ `cat $work | md5 -q` != $sum ]; then
	echo "FAIL"
else
	if [ `cat /dev/ggate${us} | md5 -q` != $sum ]; then
		echo "FAIL"
	else
		echo "PASS"
	fi
fi

ggatel destroy -u $us
mdconfig -d -u $us
mdconfig -d -u `expr $us + 1`
