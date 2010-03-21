#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/03.t,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

desc="unlink returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 create ${path1023} 0644
expect 0 unlink ${path1023}
expect ENOENT unlink ${path1023}
expect ENAMETOOLONG unlink ${path1024}
expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
