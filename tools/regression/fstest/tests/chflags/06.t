#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chflags/06.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

desc="chflags returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP chflags ${n0}/test UF_IMMUTABLE
expect ELOOP chflags ${n1}/test UF_IMMUTABLE
expect 0 unlink ${n0}
expect 0 unlink ${n1}
