#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

echo '1..1'

us=45
tsize=6
src=`mktemp /tmp/$base.XXXXXX` || exit 1
dst=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=${src} bs=1m count=$tsize >/dev/null 2>&1

mdconfig -a -t malloc -s 1M -u $us || exit 1
mdconfig -a -t malloc -s 2M -u `expr $us + 1` || exit 1
mdconfig -a -t malloc -s 3M -u `expr $us + 2` || exit 1

gconcat create $name /dev/md${us} /dev/md`expr $us + 1` /dev/md`expr $us + 2` || exit 1
devwait

dd if=${src} of=/dev/concat/${name} bs=1m count=$tsize >/dev/null 2>&1
dd if=/dev/concat/${name} of=${dst} bs=1m count=$tsize >/dev/null 2>&1

if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "not ok - md5 checksum comparison"
else
	echo "ok - md5 checksum comparison"
fi

gconcat destroy $name
mdconfig -d -u $us
mdconfig -d -u `expr $us + 1`
mdconfig -d -u `expr $us + 2`
rm -f ${src} ${dst}
