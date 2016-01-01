#!/bin/sh
# $FreeBSD$

name="$(mktemp -u concat.XXXXXX)"
class="concat"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
