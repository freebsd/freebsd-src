#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/runtests.sh,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $
#

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
