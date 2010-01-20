#!/bin/sh
# $FreeBSD$

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
