#!/bin/sh
# $FreeBSD$

name="test"
base=`basename $0`
us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`
ddbs=2048
nblocks1=1024
nblocks2=`expr $nblocks1 / \( $ddbs / 512 \)`
src=`mktemp /tmp/$base.XXXXXX` || exit 1
dst=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=${src} bs=$ddbs count=$nblocks2 >/dev/null 2>&1

mdconfig -a -t malloc -s `expr $nblocks1 + 1` -u $us0 || exit 1
mdconfig -a -t malloc -s `expr $nblocks1 + 1` -u $us1 || exit 1
mdconfig -a -t malloc -s `expr $nblocks1 + 1` -u $us2 || exit 1

graid3 label $name /dev/md${us0} /dev/md${us1} /dev/md${us2} || exit 1

dd if=${src} of=/dev/raid3/${name} bs=$ddbs count=$nblocks2 >/dev/null 2>&1

#
# Rebuild of DATA component.
#
graid3 remove -n 1 $name
dd if=/dev/zero of=/dev/md${us1} bs=512 count=`expr $nblocks1 + 1` >/dev/null 2>&1
graid3 insert -n 1 $name md${us1}
sleep 1

dd if=/dev/raid3/${name} of=${dst} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

graid3 stop $name
mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
rm -f ${src} ${dst}
