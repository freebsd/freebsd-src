#!/bin/sh
# $FreeBSD$

desc="mkfifo returns ENOENT if a component of the path prefix does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect ENOENT mkfifo ${n0}/${n1}/test 0644
expect 0 rmdir ${n0}
