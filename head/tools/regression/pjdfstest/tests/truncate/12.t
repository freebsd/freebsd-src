#!/bin/sh
# $FreeBSD$

desc="truncate returns EFBIG or EINVAL if the length argument was greater than the maximum file size"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`

expect 0 create ${n0} 0644
r=`${fstest} truncate ${n0} 999999999999999 2>/dev/null`
case "${r}" in
EFBIG|EINVAL)
	expect 0 stat ${n0} size
	;;
0)
	expect 999999999999999 stat ${n0} size
	;;
*)
	echo "not ok ${ntest}"
	ntest=`expr ${ntest} + 1`
	;;
esac
expect 0 unlink ${n0}
