#!/bin/sh
# $FreeBSD$

desc="open returns ENAMETOOLONG if an entire path name exceeded ${PATH_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

nx=`dirgen_max`
nxx="${nx}x"

mkdir -p "${nx%/*}"

expect 0 open ${nx} O_CREAT 0642
expect regular,0642 stat ${nx} type,mode
expect 0 unlink ${nx}
expect ENAMETOOLONG open ${nxx} O_CREAT 0642

rm -rf "${nx%%/*}"
