#!/bin/sh
# $FreeBSD: src/tools/regression/netinet/ipsockopt/ipsockopt.t,v 1.1 2004/11/11 19:47:53 nik Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
