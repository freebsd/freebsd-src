#!/bin/sh
# $FreeBSD: src/tools/regression/geom_stripe/runtests.sh,v 1.1.20.1 2008/10/02 02:57:24 kensmith Exp $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
