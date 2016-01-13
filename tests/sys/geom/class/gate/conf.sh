#!/bin/sh
# $FreeBSD$

name="$(mktemp -u gate.XXXXXX)"
class="gate"
base=`basename $0`

kldstat -q -m g_${class} || kldload geom_${class} || exit 1

. `dirname $0`/../geom_subr.sh
