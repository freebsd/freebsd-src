#!/bin/sh
# $FreeBSD$

name="$(mktemp -u stripe.XXXXXX)"
class="stripe"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
