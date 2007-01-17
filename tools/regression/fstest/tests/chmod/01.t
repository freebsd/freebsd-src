#!/bin/sh
# $FreeBSD$

desc="chmod returns ENOTDIR if a component of the path prefix is not a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
expect ENOTDIR chmod ${n0}/${n1}/test 0644
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}
