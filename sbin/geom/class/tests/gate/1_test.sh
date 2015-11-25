#!/bin/sh
# $FreeBSD$

. `dirname $0`/conf.sh

base=`basename $0`
us=0
while [ -c /dev/ggate${us} ]; do
	: $(( us += 1 ))
done
conf=`mktemp $base.XXXXXX` || exit 1
pidfile=ggated.pid
port=33080

work=$(attach_md -t malloc -s 1M)
src=$(attach_md -t malloc -s 1M)

test_cleanup()
{
	ggatec destroy -f -u $us
	pkill -F $pidfile
	geom_test_cleanup
}
trap test_cleanup ABRT EXIT INT TERM

dd if=/dev/random of=/dev/$work bs=1m count=1 conv=sync
dd if=/dev/random of=/dev/$src bs=1m count=1 conv=sync
src_checksum=$(md5 -q /dev/$src)

echo "127.0.0.1 RW /dev/$work" > $conf

if ! ggated -F $pidfile -p $port $conf; then
	echo 'ggated failed to start'
	echo 'Bail out!'
	exit 1
fi
if ! ggatec create -p $port -u $us 127.0.0.1 /dev/$work; then
	echo 'ggatec create failed'
	echo 'Bail out!'
	exit 1
fi

dd if=/dev/${src} of=/dev/ggate${us} bs=1m count=1
sleep 1

echo '1..2'

work_checksum=$(md5 -q /dev/$work)
if [ "$work_checksum" != "$src_checksum" ]; then
	echo "not ok 1 - md5 checksums didn't match ($work_checksum != $src_checksum)"
	echo "not ok 2 # SKIP"
else
	echo 'ok 1 - md5 checksum'

	ggate_checksum=$(md5 -q /dev/ggate${us})
	if [ "$ggate_checksum" != "$src_checksum" ]; then
		echo "not ok 2 - md5 checksums didn't match ($ggate_checksum != $src_checksum)"
	else
		echo 'ok 2 - md5 checksum'
	fi
fi
