#!/bin/sh
# $FreeBSD: src/tools/regression/netinet/ipsockopt/ipsockopt.t,v 1.1.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
