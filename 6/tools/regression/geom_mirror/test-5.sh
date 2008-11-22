#!/bin/sh
# $FreeBSD$

name="test"
base=`basename $0`
balance="split"
us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`
ddbs=8192
nblocks1=1024
nblocks2=`expr $nblocks1 / \( $ddbs / 512 \)`
src=`mktemp /tmp/$base.XXXXXX` || exit 1
dst=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=${src} bs=$ddbs count=$nblocks2 >/dev/null 2>&1

mdconfig -a -t malloc -s `expr $nblocks1 + 1` -u $us0 || exit 1
mdconfig -a -t malloc -s `expr $nblocks1 + 1` -u $us1 || exit 1
mdconfig -a -t malloc -s `expr $nblocks1 + 1` -u $us2 || exit 1

gmirror label -b $balance -s `expr $ddbs / 2` $name /dev/md${us0} /dev/md${us1} /dev/md${us2} || exit 1
sleep 1

dd if=${src} of=/dev/mirror/${name} bs=$ddbs count=$nblocks2 >/dev/null 2>&1

dd if=/dev/mirror/${name} of=${dst} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

gmirror remove $name md${us0}
dd if=/dev/mirror/${name} of=${dst} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

gmirror remove $name md${us1}
dd if=/dev/mirror/${name} of=${dst} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

gmirror remove $name md${us2}
dd if=/dev/mirror/${name} of=${dst} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

# mirror/${name} should be removed.
if [ -c /dev/${name} ]; then
	echo "FAIL"
else
	echo "PASS"
fi

mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
rm -f ${src} ${dst}
