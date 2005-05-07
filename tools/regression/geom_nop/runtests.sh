#!/bin/sh
# $FreeBSD: src/tools/regression/geom_nop/runtests.sh,v 1.1 2004/05/22 10:58:53 pjd Exp $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
