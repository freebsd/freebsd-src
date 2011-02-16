#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/04.t,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

desc="unlink returns ENOENT if the named file does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

n0=`namegen`
n1=`namegen`

expect 0 create ${n0} 0644
expect 0 unlink ${n0}
expect ENOENT unlink ${n0}
expect ENOENT unlink ${n1}
