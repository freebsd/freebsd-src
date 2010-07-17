#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/truncate/13.t,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

desc="truncate returns EINVAL if the length argument was less than 0"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

n0=`namegen`

expect 0 create ${n0} 0644
expect EINVAL -- truncate ${n0} -1
expect EINVAL -- truncate ${n0} -999999
expect 0 unlink ${n0}
