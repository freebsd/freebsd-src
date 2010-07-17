#!/bin/sh
#
# $FreeBSD: src/tools/regression/netinet/tcpfullwindowrst/tcpfullwindowrst.t,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

make tcpfullwindowrsttest 2>&1 > /dev/null

./tcpfullwindowrsttest
