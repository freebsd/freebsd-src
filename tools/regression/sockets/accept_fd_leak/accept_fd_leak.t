#!/bin/sh
# $FreeBSD: src/tools/regression/sockets/accept_fd_leak/accept_fd_leak.t,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
