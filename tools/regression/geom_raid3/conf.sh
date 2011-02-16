#!/bin/sh
# $FreeBSD: src/tools/regression/geom_raid3/conf.sh,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

name="test"
class="raid3"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
