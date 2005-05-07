#!/bin/sh
# $FreeBSD: src/tools/regression/geom_shsec/runtests.sh,v 1.1.2.1 2005/03/01 13:17:31 pjd Exp $

dir=`dirname $0`

gshsec load >/dev/null 2>&1
for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
gshsec unload
