#!/bin/sh
#
# $FreeBSD: src/tools/regression/geom_uzip/runtests.sh,v 1.1 2004/08/13 09:53:52 fjoe Exp $
#

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
