#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libutil/test-trimdomain.t,v 1.1.2.1.8.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
