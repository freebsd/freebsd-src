#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

base=`basename $0`
us=47

test_cleanup()
{
	ggatel destroy -f -u $us

	geom_test_cleanup
}
trap test_cleanup ABRT EXIT INT TERM

work=$(attach_md -t malloc -s 1M)
src=$(attach_md -t malloc -s 1M)

dd if=/dev/random of=/dev/$work bs=1m count=1 conv=sync
dd if=/dev/random of=/dev/$src bs=1m count=1 conv=sync
src_checksum=$(md5 -q /dev/$src)

if ! ggatel create -u $us /dev/$work; then
	echo 'ggatel create failed'
	echo 'Bail out!'
	exit 1
fi

sleep 1
dd if=/dev/${src} of=/dev/ggate${us} bs=1m count=1 conv=sync
sleep 1

echo '1..2'

work_checksum=$(md5 -q /dev/$work)
if [ "$work_checksum" != "$src_checksum" ]; then
	echo "not ok 1 - md5 checksums didn't match ($work_checksum != $src_checksum)"
	echo 'not ok 2 # SKIP'
else
	echo 'ok 1 - md5 checksum'

	ggate_checksum=$(md5 -q /dev/ggate${us})
	if [ "$ggate_checksum" != "$src_checksum" ]; then
		echo "not ok 2 - md5 checksums didn't match ($ggate_checksum != $src_checksum)"
	else
		echo 'ok 2 - md5 checksum'
	fi
fi
