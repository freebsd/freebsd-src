#!/bin/sh
# $FreeBSD$

desc="link returns ENAMETOOLONG if an entire length of either path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..16"

n0=`namegen`

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 create ${path1023} 0644
expect 0 link ${path1023} ${n0}
expect 0 unlink ${path1023}
expect 0 link ${n0} ${path1023}
expect 0 unlink ${path1023}
expect ENAMETOOLONG link ${n0} ${path1024}
expect 0 unlink ${n0}
expect ENAMETOOLONG link ${path1024} ${n0}
expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
