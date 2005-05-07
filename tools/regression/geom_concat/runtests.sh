#!/bin/sh
# $FreeBSD: src/tools/regression/geom_concat/runtests.sh,v 1.1 2004/03/03 21:52:49 pjd Exp $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
