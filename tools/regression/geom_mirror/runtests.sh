#!/bin/sh
# $FreeBSD: src/tools/regression/geom_mirror/runtests.sh,v 1.1 2004/07/30 23:13:45 pjd Exp $

dir=`dirname $0`

gmirror load >/dev/null 2>&1
for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
gmirror unload >/dev/null 2>&1
