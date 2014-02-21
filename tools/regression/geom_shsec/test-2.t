#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

echo "1..4"

us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`
nblocks1=1024
nblocks2=`expr $nblocks1 + 1`
src=`mktemp /tmp/$base.XXXXXX` || exit 1
dst=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=${src} count=$nblocks1 >/dev/null 2>&1

mdconfig -a -t malloc -s $nblocks2 -u $us0 || exit 1
mdconfig -a -t malloc -s $nblocks2 -u $us1 || exit 1
mdconfig -a -t malloc -s $nblocks2 -u $us2 || exit 1

gshsec label $name /dev/md${us0} /dev/md${us1} /dev/md${us2} || exit 1
devwait

dd if=${src} of=/dev/shsec/${name} count=$nblocks1 >/dev/null 2>&1

dd if=/dev/shsec/${name} of=${dst} count=$nblocks1 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "not ok 1"
else
	echo "ok 1"
fi

dd if=/dev/md${us0} of=${dst} count=$nblocks1 >/dev/null 2>&1
if [ `md5 -q ${src}` = `md5 -q ${dst}` ]; then
	echo "not ok 2"
else
	echo "ok 2"
fi

dd if=/dev/md${us1} of=${dst} count=$nblocks1 >/dev/null 2>&1
if [ `md5 -q ${src}` = `md5 -q ${dst}` ]; then
	echo "not ok 3"
else
	echo "ok 3"
fi

dd if=/dev/md${us2} of=${dst} count=$nblocks1 >/dev/null 2>&1
if [ `md5 -q ${src}` = `md5 -q ${dst}` ]; then
	echo "not ok 4"
else
	echo "ok 4"
fi

gshsec stop $name
mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
rm -f ${src} ${dst}
