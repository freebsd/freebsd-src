#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chown/02.t,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

desc="chown returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

expect 0 create ${name255} 0644
expect 0 chown ${name255} 65534 65534
expect 65534,65534 stat ${name255} uid,gid
expect 0 unlink ${name255}
expect ENAMETOOLONG chown ${name256} 65533 65533
