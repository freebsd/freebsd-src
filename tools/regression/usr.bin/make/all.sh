#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/make/all.sh,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

# find all test scripts below our current directory
SCRIPTS=`find . -name test.t`

if [ -z "${SCRIPTS}" ] ; then
	exit 0
fi

for i in ${SCRIPTS} ; do
	(
	cd `dirname $i`
	sh ./test.t $1
	)
done
