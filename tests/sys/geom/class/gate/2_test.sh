#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

base=`basename $0`
us=46
work=`mktemp -u $base.XXXXXX` || exit 1
src=`mktemp -u $base.XXXXXX` || exit 1

test_cleanup()
{
	ggatel destroy -f -u $us
	rm -f $work $src

	geom_test_cleanup
}
trap test_cleanup ABRT EXIT INT TERM

dd if=/dev/random of=$work bs=1m count=1 conv=sync
dd if=/dev/random of=$src bs=1m count=1 conv=sync

if ! ggatel create -u $us $work; then
	echo 'ggatel create failed'
	echo 'Bail out!'
	exit 1
fi

dd if=${src} of=/dev/ggate${us} bs=1m count=1
sleep 1

echo '1..2'

src_checksum=$(md5 -q $src)
work_checksum=$(md5 -q $work)
if [ "$work_checksum" != "$src_checksum" ]; then
	echo "not ok 1 - md5 checksums didn't match ($work_checksum != $src_checksum) # TODO: bug 204616"
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
