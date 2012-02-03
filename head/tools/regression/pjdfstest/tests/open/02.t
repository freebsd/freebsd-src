#!/bin/sh
# $FreeBSD$

desc="open returns ENAMETOOLONG if a component of a pathname exceeded {NAME_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

nx=`namegen_max`
nxx="${nx}x"

expect 0 open ${nx} O_CREAT 0620
expect regular,0620 stat ${nx} type,mode
expect 0 unlink ${nx}
expect ENAMETOOLONG open ${nxx} O_CREAT 0620
