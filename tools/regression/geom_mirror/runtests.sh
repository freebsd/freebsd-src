#!/bin/sh
# $FreeBSD$

dir=`dirname $0`

gmirror load >/dev/null 2>&1
for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
gmirror unload >/dev/null 2>&1
