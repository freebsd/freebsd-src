#!/bin/sh

. `dirname $0`/conf.sh

echo "1..1"

ddbs=2048
nblocks1=1024
nblocks2=`expr $nblocks1 / \( $ddbs / 512 \)`
src=`mktemp $base.XXXXXX` || exit 1
dst=`mktemp $base.XXXXXX` || exit 1

attach_md us0 -t malloc -s $(expr $nblocks1 + 1) || exit 1
attach_md us1 -t malloc -s $(expr $nblocks1 + 1) || exit 1
attach_md us2 -t malloc -s $(expr $nblocks1 + 1) || exit 1

dd if=/dev/random of=${src} bs=$ddbs count=$nblocks2 >/dev/null 2>&1

graid3 label $name /dev/${us0} /dev/${us1} /dev/${us2} || exit 1
devwait

#
# Writing without DATA component and rebuild of DATA component.
#
graid3 remove -n 1 $name
dd if=/dev/zero of=/dev/${us1} bs=512 count=`expr $nblocks1 + 1` >/dev/null 2>&1
dd if=${src} of=/dev/raid3/${name} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
graid3 insert -n 1 $name md${us1}
sleep 1

dd if=/dev/raid3/${name} of=${dst} bs=$ddbs count=$nblocks2 >/dev/null 2>&1
if [ `md5 -q ${src}` != `md5 -q ${dst}` ]; then
	echo "not ok 1"
else
	echo "ok 1"
fi

rm -f ${src} ${dst}
