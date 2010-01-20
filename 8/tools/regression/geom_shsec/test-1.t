#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

echo "1..2"

us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`

mdconfig -a -t malloc -s 1M -u $us0 || exit 1
mdconfig -a -t malloc -s 2M -u $us1 || exit 1
mdconfig -a -t malloc -s 3M -u $us2 || exit 1

gshsec label $name /dev/md${us0} /dev/md${us1} /dev/md${us2} 2>/dev/null || exit 1
devwait

# Size of created device should be 1MB - 512B.

mediasize=`diskinfo /dev/shsec/${name} | awk '{print $3}'`
if [ $mediasize -eq 1048064 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
sectorsize=`diskinfo /dev/shsec/${name} | awk '{print $2}'`
if [ $sectorsize -eq 512 ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi

gshsec stop $name
mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
