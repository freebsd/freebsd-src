#!/bin/sh
# $FreeBSD$

set -e
MD=`mdconfig -a -t malloc -s 4m -x 63 -y 16`
if [ ! -c /dev/${MD} ] ; then
	echo "MD device $MD did not materialize" 1>&2
	exit 2
fi
trap "mdconfig -d -u ${MD}" EXIT INT TERM

# Create an empty bootcode file to isolate our checksum from any changes
# which might happen to the boot code file.
dd if=/dev/zero of=tmp count=1 > /dev/null 2>&1
./fdisk -b tmp -I $MD > /dev/null 2>&1
rm tmp

c=`dd if=/dev/${MD} count=1 2>/dev/null | md5`
if [ $c != 509b44919d3921502bd31237c4bb1f75 ] ; then
	echo "FAILED: fdisk -I gives bad checksum" 1>&2
	exit 1
fi
echo "PASSED: fdisk -I"
exit 0
