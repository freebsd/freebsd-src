#!/bin/sh
# $FreeBSD$

desc="unlink returns ENAMETOOLONG if a component of a pathname exceeded {NAME_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

nx=`namegen_max`
nxx="${nx}x"

expect 0 create ${nx} 0644
expect 0 unlink ${nx}
expect ENOENT unlink ${nx}
expect ENAMETOOLONG unlink ${nxx}
