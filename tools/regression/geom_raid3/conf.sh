#!/bin/sh
# $FreeBSD$

name="$(mktemp -u graid3.XXXXXX)"
class="raid3"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
