#!/bin/sh
# $FreeBSD: src/tools/regression/geom_nop/test-2.sh,v 1.1 2004/05/22 10:58:53 pjd Exp $

name="test"
base=`basename $0`
us=45
src=`mktemp /tmp/$base.XXXXXX` || exit 1
dst=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=${src} bs=1m count=1 >/dev/null 2>&1

mdconfig -a -t malloc -s 1M -u $us || exit 1

gnop create /dev/md${us} || exit 1

dd if=${src} of=/dev/md${us}.nop bs=1m count=1 >/dev/null 2>&1
dd if=/dev/md${us}.nop of=${dst} bs=1m count=1 >/dev/null 2>&1

if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "FAIL"
else
	echo "PASS"
fi

gnop destroy md${us}.nop
mdconfig -d -u $us
rm -f ${src} ${dst}
