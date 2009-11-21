#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libutil/test-trimdomain-nodomain.t,v 1.1.12.1.2.1 2009/10/25 01:10:29 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
