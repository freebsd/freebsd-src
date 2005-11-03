#!/bin/sh
# $FreeBSD: src/tools/regression/geom_gate/runtests.sh,v 1.1 2004/05/03 18:29:54 pjd Exp $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
