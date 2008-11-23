#!/bin/sh
# $FreeBSD$

desc="unlink may return EPERM if the named file is a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`

expect 0 mkdir ${n0} 0755
expect "0|EPERM" unlink ${n0}
expect "0|ENOENT" rmdir ${n0}
