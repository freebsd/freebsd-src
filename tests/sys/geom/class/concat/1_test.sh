#!/bin/sh

. `dirname $0`/conf.sh

echo '1..1'

attach_md us0 -t malloc -s 1M || exit 1
attach_md us1 -t malloc -s 2M || exit 1
attach_md us2 -t malloc -s 3M || exit 1

gconcat create $name /dev/$us0 /dev/$us1 /dev/$us2 || exit 1
devwait

# Size of created device should be 1MB + 2MB + 3MB.

size=`diskinfo /dev/concat/${name} | awk '{print $3}'`

if [ $size -eq 6291456 ]; then
	echo "ok - Size is 6291456"
else
	echo "not ok - Size is 6291456"
fi
