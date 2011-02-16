#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rename/18.t,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

desc="rename returns EINVAL when the 'from' argument is a parent directory of 'to'"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n0}/${n1} 0755

expect EINVAL rename ${n0} ${n0}/${n1}
expect EINVAL rename ${n0} ${n0}/${n1}/${n2}

expect 0 rmdir ${n0}/${n1}
expect 0 rmdir ${n0}
