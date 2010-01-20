#!/bin/sh
# $FreeBSD$

desc="link returns ENOENT if the source file does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

n0=`namegen`
n1=`namegen`

expect 0 create ${n0} 0644
expect 0 link ${n0} ${n1}
expect 0 unlink ${n0}
expect 0 unlink ${n1}
expect ENOENT link ${n0} ${n1}
