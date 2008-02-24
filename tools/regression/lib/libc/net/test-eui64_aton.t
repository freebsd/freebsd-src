#!/bin/sh
# $FreeBSD: src/tools/regression/lib/libc/net/test-eui64_aton.t,v 1.1 2004/11/11 19:47:52 nik Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
