#!/bin/sh
# $FreeBSD: src/tools/regression/sockets/accept_fd_leak/accept_fd_leak.t,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
