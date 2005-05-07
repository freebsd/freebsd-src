#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/test-2.sh,v 1.1 2004/08/13 09:53:52 fjoe Exp $
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
