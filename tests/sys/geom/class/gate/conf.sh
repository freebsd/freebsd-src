#!/bin/sh
# $FreeBSD$

name="$(mktemp -u gate.XXXXXX)"
class="gate"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
