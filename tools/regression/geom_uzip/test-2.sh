#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/test-2.sh,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $
#

#
# prepare
kldload geom_uzip
uudecode test-1.img.uzip.uue
num=`mdconfig -an -f test-1.img.uzip` || exit 1
sleep 1

#
# destroy
kldunload geom_uzip
