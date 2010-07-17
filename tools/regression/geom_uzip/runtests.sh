#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/runtests.sh,v 1.1.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
