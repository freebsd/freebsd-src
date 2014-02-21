#!/bin/sh
# $FreeBSD$

desc="mkdir returns ENAMETOOLONG if an entire path name exceeded {PATH_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

nx=`dirgen_max`
nxx="${nx}x"

mkdir -p "${nx%/*}"

expect 0 mkdir ${nx} 0755
expect 0 rmdir ${nx}
expect ENAMETOOLONG mkdir ${nxx} 0755

rm -rf "${nx%%/*}"
