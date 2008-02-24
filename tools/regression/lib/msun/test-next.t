#!/bin/sh
# $FreeBSD: src/tools/regression/lib/msun/test-next.t,v 1.1 2005/03/07 05:03:46 das Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
