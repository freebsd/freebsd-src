#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkdir/02.t,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

desc="mkdir returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

expect 0 mkdir ${name255} 0755
expect 0 rmdir ${name255}
expect ENAMETOOLONG mkdir ${name256} 0755
