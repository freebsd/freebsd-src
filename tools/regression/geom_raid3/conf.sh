#!/bin/sh
# $FreeBSD: src/tools/regression/geom_raid3/conf.sh,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

name="test"
class="raid3"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
