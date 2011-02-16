#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/04.t,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

desc="rmdir returns ENOENT if the named directory does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 rmdir ${n0}
expect ENOENT rmdir ${n0}
expect ENOENT rmdir ${n1}
