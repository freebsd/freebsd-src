#!/bin/sh
#
# $FreeBSD: src/tools/regression/netinet/tcpfullwindowrst/tcpfullwindowrst.t,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

make tcpfullwindowrsttest 2>&1 > /dev/null

./tcpfullwindowrsttest
