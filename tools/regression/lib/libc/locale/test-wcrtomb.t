#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libc/locale/test-wcrtomb.t,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
