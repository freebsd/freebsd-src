#!/bin/sh
# $FreeBSD$

name="test"
base=`basename $0`
us=45

echo '1..1'

mdconfig -a -t malloc -s 1M -u $us || exit 1
mdconfig -a -t malloc -s 2M -u `expr $us + 1` || exit 1
mdconfig -a -t malloc -s 3M -u `expr $us + 2` || exit 1

gconcat create $name /dev/md${us} /dev/md`expr $us + 1` /dev/md`expr $us + 2` || exit 1

# Size of created device should be 1MB + 2MB + 3MB.

size=`diskinfo /dev/concat/${name} | awk '{print $3}'`

if [ $size -eq 6291456 ]; then
	echo "ok - Size is 6291456"
else
	echo "not ok - Size is 6291456"
fi

gconcat destroy $name
mdconfig -d -u $us
mdconfig -d -u `expr $us + 1`
mdconfig -d -u `expr $us + 2`
