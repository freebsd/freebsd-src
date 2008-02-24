#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libmp/test-libmp.t,v 1.1 2006/07/28 16:00:59 simon Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
