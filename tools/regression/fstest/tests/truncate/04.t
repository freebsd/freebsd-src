#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/truncate/04.t,v 1.1 2007/01/17 01:42:12 pjd Exp $

desc="truncate returns ENOENT if the named file does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect ENOENT truncate ${n0}/${n1}/test 123
expect ENOENT truncate ${n0}/${n1} 123
expect 0 rmdir ${n0}
