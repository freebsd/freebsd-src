#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/test-2.sh,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $
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
