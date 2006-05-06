#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libutil/test-trimdomain.t,v 1.1.2.1 2005/12/22 03:47:05 brooks Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
