#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
keyfile=`mktemp $base.XXXXXX` || exit 1
md=$(attach_md -t malloc -s `expr $sectors + 1`)

echo "1..11"

dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

geli init -B none -P -K $keyfile ${md}
if [ $? -eq 0 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi

geli attach -r -p -k $keyfile ${md}
if [ $? -eq 0 ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi

sh -c "true >/dev/${md}.eli" 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

geli kill ${md}
if [ $? -eq 0 ]; then
	echo "ok 4"
else
	echo "not ok 4"
fi

# kill should detach provider...
if [ ! -c /dev/${md}.eli ]; then
	echo "ok 5"
else
	echo "not ok 5"
fi

# ...but not destroy the metadata.
geli attach -r -p -k $keyfile ${md}
if [ $? -eq 0 ]; then
	echo "ok 6"
else
	echo "not ok 6"
fi

geli setkey -n 1 -P -K /dev/null ${md} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 7"
else
	echo "not ok 7"
fi

geli delkey -n 0 ${md} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 8"
else
	echo "not ok 8"
fi

geli delkey -f -n 0 ${md} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 9"
else
	echo "not ok 9"
fi

geli list ${md}.eli | egrep '^Flags: .*READ-ONLY' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 10"
else
	echo "not ok 10"
fi

geli detach ${md}
if [ $? -eq 0 ]; then
	echo "ok 11"
else
	echo "not ok 11"
fi

mdconfig -d -u ${md}
rm -f $keyfile
