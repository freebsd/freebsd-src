#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/03.t,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

desc="open returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 open ${path1023} O_CREAT 0642
expect 0642 stat ${path1023} mode
expect 0 unlink ${path1023}
expect ENAMETOOLONG open ${path1024} O_CREAT 0642
expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
