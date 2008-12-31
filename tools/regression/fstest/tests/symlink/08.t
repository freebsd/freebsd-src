#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/symlink/08.t,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

desc="symlink returns EEXIST if the name2 argument already exists"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..9"

n0=`namegen`

expect 0 create ${n0} 0644
expect EEXIST symlink test ${n0}
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
expect EEXIST symlink test ${n0}
expect 0 rmdir ${n0}

expect 0 symlink test ${n0}
expect EEXIST symlink test ${n0}
expect 0 unlink ${n0}
