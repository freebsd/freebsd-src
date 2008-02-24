#!/bin/sh
# $FreeBSD: src/tools/regression/file/dup/dup.t,v 1.1 2006/11/11 18:32:50 maxim Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
