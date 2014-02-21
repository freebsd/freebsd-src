#!/bin/sh
# $FreeBSD$

desc="truncate returns EISDIR if the named file is a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`

expect 0 mkdir ${n0} 0755
expect EISDIR truncate ${n0} 123
expect 0 rmdir ${n0}
