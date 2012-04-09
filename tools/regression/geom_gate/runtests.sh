#!/bin/sh
# $FreeBSD: src/tools/regression/geom_gate/runtests.sh,v 1.1.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
