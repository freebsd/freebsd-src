#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/test-2.sh,v 1.1.20.1 2008/10/02 02:57:24 kensmith Exp $
#

#
# prepare
kldload geom_uzip
uudecode test-1.img.gz.uue
num=`mdconfig -an -f test-1.img.gz` || exit 1
sleep 1

#
# destroy
kldunload geom_uzip
