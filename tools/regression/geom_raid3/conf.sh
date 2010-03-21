#!/bin/sh
# $FreeBSD: src/tools/regression/geom_raid3/conf.sh,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

name="test"
class="raid3"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
