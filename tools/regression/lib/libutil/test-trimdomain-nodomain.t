#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libutil/test-trimdomain-nodomain.t,v 1.1 2005/10/05 04:46:10 brooks Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
