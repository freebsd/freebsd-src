#!/bin/sh
# $FreeBSD: src/tools/regression/bin/sh/regress.t,v 1.1.10.3.4.1 2012/03/03 06:15:13 kensmith Exp $

export SH="${SH:-sh}"

cd `dirname $0`

${SH} regress.sh
