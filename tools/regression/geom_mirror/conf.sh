#!/bin/sh
# $FreeBSD$

name="$(mktemp -u mirror.XXXXXX)"
class="mirror"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
