#!/bin/sh
# $FreeBSD: src/tools/regression/lib/msun/test-lround.t,v 1.1 2005/01/11 23:13:36 das Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
