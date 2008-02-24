#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libc/net/test-ether.t,v 1.1 2007/05/13 14:03:21 rwatson Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
