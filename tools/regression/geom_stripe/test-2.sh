#!/bin/sh
# $FreeBSD$

name="test"
base=`basename $0`
us=45
tsize=3
src=`mktemp /tmp/$base.XXXXXX` || exit 1
dst=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=${src} bs=1m count=$tsize >/dev/null 2>&1

mdconfig -a -t malloc -s 1M -u $us || exit 1
mdconfig -a -t malloc -s 2M -u `expr $us + 1` || exit 1
mdconfig -a -t malloc -s 3M -u `expr $us + 2` || exit 1

gstripe create -s 8192 $name /dev/md${us} /dev/md`expr $us + 1` /dev/md`expr $us + 2` || exit 1

dd if=${src} of=/dev/stripe/${name} bs=1m count=$tsize >/dev/null 2>&1
dd if=/dev/stripe/${name} of=${dst} bs=1m count=$tsize >/dev/null 2>&1

if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

gstripe destroy $name
mdconfig -d -u $us
mdconfig -d -u `expr $us + 1`
mdconfig -d -u `expr $us + 2`
rm -f ${src} ${dst}
