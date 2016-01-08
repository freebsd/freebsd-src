#!/bin/sh
# $FreeBSD$

testsdir=$(dirname $0)
. $testsdir/conf.sh

echo "1..1"

UUE=$testsdir/test-1.img.uzip.uue
uudecode $UUE
us0=$(attach_md -f $(basename $UUE .uue)) || exit 1
sleep 1

mount -o ro /dev/${us0}.uzip "${mntpoint}" || exit 1

#cat "${mntpoint}/etalon.txt"
diff -I '\$FreeBSD.*\$' -u $testsdir/etalon/etalon.txt "${mntpoint}/etalon.txt"
if [ $? -eq 0 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
