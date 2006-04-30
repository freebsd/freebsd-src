#!/bin/sh
#
# $FreeBSD$
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
