#!/bin/sh
# $FreeBSD: src/tools/regression/geom_raid3/runtests.sh,v 1.1 2004/08/16 09:09:23 pjd Exp $

dir=`dirname $0`

graid3 load >/dev/null 2>&1
for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
graid3 unload
