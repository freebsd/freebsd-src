#!/bin/sh
# $FreeBSD: src/tools/regression/sockets/accf_data_attach/accf_data_attach.t,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
