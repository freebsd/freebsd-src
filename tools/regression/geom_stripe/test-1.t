#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

echo "1..1"

us=45

mdconfig -a -t malloc -s 1M -u $us || exit 1
mdconfig -a -t malloc -s 2M -u `expr $us + 1` || exit 1
mdconfig -a -t malloc -s 3M -u `expr $us + 2` || exit 1

gstripe create -s 16384 $name /dev/md${us} /dev/md`expr $us + 1` /dev/md`expr $us + 2` || exit 1
devwait

# Size of created device should be 1MB * 3.

size=`diskinfo /dev/stripe/${name} | awk '{print $3}'`

if [ $size -eq 3145728 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi

gstripe destroy $name
mdconfig -d -u $us
mdconfig -d -u `expr $us + 1`
mdconfig -d -u `expr $us + 2`
