#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rename/01.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="rename returns ENAMETOOLONG if a component of either pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..8"

n0=`namegen`

expect 0 create ${name255} 0644
expect 0 rename ${name255} ${n0}
expect 0 rename ${n0} ${name255}
expect 0 unlink ${name255}

expect 0 create ${n0} 0644
expect ENAMETOOLONG rename ${n0} ${name256}
expect 0 unlink ${n0}
expect ENAMETOOLONG rename ${name256} ${n0}
