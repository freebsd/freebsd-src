#!/bin/sh
# $FreeBSD: src/tools/regression/file/closefrom/closefrom.t,v 1.1.4.1.6.1 2010/12/21 17:09:25 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
