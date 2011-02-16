#!/bin/sh
# $FreeBSD: src/tools/regression/sockets/accf_data_attach/accf_data_attach.t,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
