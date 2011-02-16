#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/runtests.sh,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $
#

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
