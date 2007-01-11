#!/bin/sh
# $FreeBSD: src/tools/regression/lib/msun/test-rem.t,v 1.1 2005/04/02 12:50:28 das Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
