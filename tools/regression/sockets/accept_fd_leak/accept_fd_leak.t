#!/bin/sh
# $FreeBSD: src/tools/regression/sockets/accept_fd_leak/accept_fd_leak.t,v 1.1 2004/11/11 19:47:53 nik Exp $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
