#!/bin/sh
# $FreeBSD: src/tools/regression/geom_gate/runtests.sh,v 1.1.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
