#!/bin/sh
# $FreeBSD$

desc="truncate returns ENAMETOOLONG if a component of a pathname exceeded {NAME_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

nx=`namegen_max`
nxx="${nx}x"

expect 0 create ${nx} 0644
expect 0 truncate ${nx} 123
expect 123 stat ${nx} size
expect 0 unlink ${nx}
expect ENAMETOOLONG truncate ${nxx} 123
