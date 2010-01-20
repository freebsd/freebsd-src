#!/bin/sh
# $FreeBSD$

dir=`dirname $0`

graid3 load >/dev/null 2>&1
for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
graid3 unload
