#!/bin/sh
# $FreeBSD$

name="test"
base=`basename $0`
us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`

mdconfig -a -t malloc -s 1M -u $us0 || exit 1
mdconfig -a -t malloc -s 2M -u $us1 || exit 1
mdconfig -a -t malloc -s 3M -u $us2 || exit 1

gshsec label $name /dev/md${us0} /dev/md${us1} /dev/md${us2} || exit 1

# Size of created device should be 1MB - 512B.

mediasize=`diskinfo /dev/shsec/${name} | awk '{print $3}'`
if [ $mediasize -eq 1048064 ]; then
	echo "PASS"
else
	echo "FAIL"
fi
sectorsize=`diskinfo /dev/shsec/${name} | awk '{print $2}'`
if [ $sectorsize -eq 512 ]; then
	echo "PASS"
else
	echo "FAIL"
fi

gshsec stop $name
mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
