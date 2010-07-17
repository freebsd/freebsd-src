#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/test-2.sh,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $
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
