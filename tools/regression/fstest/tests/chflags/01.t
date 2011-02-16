#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chflags/01.t,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

desc="chflags returns ENOTDIR if a component of the path prefix is not a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..5"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
expect ENOTDIR chflags ${n0}/${n1}/test SF_IMMUTABLE
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}
