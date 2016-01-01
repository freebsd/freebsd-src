#!/bin/sh
# $FreeBSD$

name="$(mktemp -u shsec.XXXXXX)"
class="shsec"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
