#!/bin/sh
#
# $FreeBSD: src/tools/regression/netinet/tcpfullwindowrst/tcpfullwindowrst.t,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

make tcpfullwindowrsttest 2>&1 > /dev/null

./tcpfullwindowrsttest
