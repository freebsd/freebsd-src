#!/bin/sh
# $FreeBSD: src/tools/regression/geom_raid3/conf.sh,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

name="test"
class="raid3"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
