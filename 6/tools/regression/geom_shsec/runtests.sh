#!/bin/sh
# $FreeBSD$

dir=`dirname $0`

gshsec load >/dev/null 2>&1
for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
gshsec unload
