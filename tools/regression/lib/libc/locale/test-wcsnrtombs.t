#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libc/locale/test-wcsnrtombs.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
