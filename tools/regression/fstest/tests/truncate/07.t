#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/truncate/07.t,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

desc="truncate returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP truncate ${n0}/test 123
expect ELOOP truncate ${n1}/test 123
expect 0 unlink ${n0}
expect 0 unlink ${n1}
