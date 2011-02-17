#!/bin/sh
# $FreeBSD$

desc="mkfifo returns ENAMETOOLONG if an entire path name exceeded {PATH_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

nx=`dirgen_max`
nxx="${nx}x"

mkdir -p "${nx%/*}"

expect 0 mkfifo ${nx} 0644
expect fifo stat ${nx} type
expect 0 unlink ${nx}
expect ENAMETOOLONG mkfifo ${nxx} 0644

rm -rf "${nx%%/*}"
