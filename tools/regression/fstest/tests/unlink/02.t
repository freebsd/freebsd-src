#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/02.t,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

desc="unlink returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

expect 0 create ${name255} 0644
expect 0 unlink ${name255}
expect ENOENT unlink ${name255}
expect ENAMETOOLONG unlink ${name256}
