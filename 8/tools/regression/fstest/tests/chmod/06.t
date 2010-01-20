#!/bin/sh
# $FreeBSD$

desc="chmod returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP chmod ${n0}/test 0644
expect ELOOP chmod ${n1}/test 0644
expect 0 unlink ${n0}
expect 0 unlink ${n1}
