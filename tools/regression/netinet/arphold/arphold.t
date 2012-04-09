#!/bin/sh
#
# $FreeBSD: src/tools/regression/netinet/arphold/arphold.t,v 1.1.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $

make arphold 2>&1 > /dev/null

./arphold 192.168.1.222
