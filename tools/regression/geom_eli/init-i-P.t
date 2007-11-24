#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
keyfile=`mktemp /tmp/$base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..1"

dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

geli init -i 64 -P -K ${keyfile} md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi

mdconfig -d -u $no
rm -f $keyfile
