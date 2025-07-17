#!/bin/sh

. `dirname $0`/conf.sh

echo "1..1"

tsize=3
src=`mktemp $base.XXXXXX` || exit 1
dst=`mktemp $base.XXXXXX` || exit 1

dd if=/dev/random of=${src} bs=1m count=$tsize >/dev/null 2>&1

attach_md us0 -t malloc -s 1M || exit 1
attach_md us1 -t malloc -s 2M || exit 1
attach_md us2 -t malloc -s 3M || exit 1

gstripe create -s 8192 $name /dev/$us0 /dev/$us1 /dev/$us2 || exit 1
devwait

dd if=${src} of=/dev/stripe/${name} bs=1m count=$tsize >/dev/null 2>&1
dd if=/dev/stripe/${name} of=${dst} bs=1m count=$tsize >/dev/null 2>&1

if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "not ok 1"
else
	echo "ok 1"
fi

rm -f ${src} ${dst}
