#!/bin/sh
#
# $FreeBSD$
#

#
# prepare

UUE=$(dirname $0)/1.img.uzip.uue

kldload geom_uzip
uudecode $UUE
num=`mdconfig -an -f $(basename $UUE .uue)` || exit 1
sleep 1

#
# destroy
kldunload geom_uzip
