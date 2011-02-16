#!/bin/sh
#
# $FreeBSD: src/tools/regression/netinet/tcpfullwindowrst/tcpfullwindowrst.t,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

make tcpfullwindowrsttest 2>&1 > /dev/null

./tcpfullwindowrsttest
