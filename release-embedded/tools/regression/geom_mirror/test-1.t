#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

echo "1..1"

us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`

mdconfig -a -t malloc -s 1M -u $us0 || exit 1
mdconfig -a -t malloc -s 2M -u $us1 || exit 1
mdconfig -a -t malloc -s 3M -u $us2 || exit 1

gmirror label $name /dev/md${us0} /dev/md${us1} /dev/md${us2} || exit 1
devwait

# Size of created device should be 1MB - 512b.

size=`diskinfo /dev/mirror/${name} | awk '{print $3}'`

if [ $size -eq 1048064 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi

gmirror remove $name md${us0}
gmirror remove $name md${us1}
gmirror remove $name md${us2}
mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
