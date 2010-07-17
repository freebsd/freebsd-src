#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libmp/test-libmp.t,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
