#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

src=`mktemp $base.XXXXXX` || exit 1
dst=`mktemp $base.XXXXXX` || exit 1

echo "1..1"

dd if=/dev/random of=${src} bs=1m count=1 >/dev/null 2>&1

us=$(attach_md -t malloc -s 1M) || exit 1

gnop create /dev/${us} || exit 1

dd if=${src} of=/dev/${us}.nop bs=1m count=1 >/dev/null 2>&1
dd if=/dev/${us}.nop of=${dst} bs=1m count=1 >/dev/null 2>&1

if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "not ok 1"
else
	echo "ok 1"
fi

rm -f ${src} ${dst}
