#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chflags/06.t,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

desc="chflags returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP chflags ${n0}/test SF_IMMUTABLE
expect ELOOP chflags ${n1}/test SF_IMMUTABLE
expect 0 unlink ${n0}
expect 0 unlink ${n1}
