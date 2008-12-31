#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/runtests.sh,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $
#

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
