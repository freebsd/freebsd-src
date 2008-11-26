#!/bin/sh
#
# $FreeBSD: src/tools/regression/netinet/tcpfullwindowrst/tcpfullwindowrst.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

make tcpfullwindowrsttest 2>&1 > /dev/null

./tcpfullwindowrsttest
