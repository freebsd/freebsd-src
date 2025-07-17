#!/bin/sh

. `dirname $0`/conf.sh

echo "1..2"

attach_md us0 -t malloc -s 1M || exit 1
attach_md us1 -t malloc -s 2M || exit 1
attach_md us2 -t malloc -s 3M || exit 1

graid3 label $name /dev/${us0} /dev/${us1} /dev/${us2} 2>/dev/null || exit 1
devwait

# Size of created device should be 2MB - 1024B.

mediasize=`diskinfo /dev/raid3/${name} | awk '{print $3}'`
if [ $mediasize -eq 2096128 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
sectorsize=`diskinfo /dev/raid3/${name} | awk '{print $2}'`
if [ $sectorsize -eq 1024 ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi
