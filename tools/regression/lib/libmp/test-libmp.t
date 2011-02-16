#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libmp/test-libmp.t,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
