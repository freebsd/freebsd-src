#!/bin/sh
# $FreeBSD$

desc="chflags returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..13"

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 create ${path1023} 0644
expect 0 chflags ${path1023} UF_IMMUTABLE
expect 0 chflags ${path1023} none
expect 0 unlink ${path1023}
expect ENAMETOOLONG chflags ${path1024} UF_IMMUTABLE
expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
