#!/bin/sh
# $FreeBSD$

desc="chmod returns ENAMETOOLONG if an entire path name exceeded {PATH_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..10"

nx=`dirgen_max`
nxx="${nx}x"

mkdir -p "${nx%/*}"

expect 0 create ${nx} 0644
expect 0 chmod ${nx} 0642
expect 0642 stat ${nx} mode
expect 0 unlink ${nx}
expect ENAMETOOLONG chmod ${nxx} 0642

expect 0 create ${nx} 0644
expect 0 lchmod ${nx} 0642
expect 0642 stat ${nx} mode
expect 0 unlink ${nx}
expect ENAMETOOLONG lchmod ${nxx} 0642

rm -rf "${nx%%/*}"
