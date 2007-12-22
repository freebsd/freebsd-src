#!/bin/sh
# $FreeBSD$

desc="unlink returns EPERM if the named file is a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`

expect 0 mkdir ${n0} 0755
case "${os}:${fs}" in
SunOS:UFS)
	expect 0 unlink ${n0}
	expect ENOENT rmdir ${n0}
	;;
*)
	expect EPERM unlink ${n0}
	expect 0 rmdir ${n0}
	;;
esac
