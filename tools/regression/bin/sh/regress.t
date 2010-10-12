#!/bin/sh
# $FreeBSD$

if [ -z "${SH}" ]; then
	echo '${SH} is not set, please correct and re-run.'
	exit 1
fi
export SH=${SH}

cd `dirname $0`

${SH} regress.sh
