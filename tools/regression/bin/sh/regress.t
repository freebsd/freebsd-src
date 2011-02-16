#!/bin/sh
# $FreeBSD: src/tools/regression/bin/sh/regress.t,v 1.1.10.3.2.1 2010/12/21 17:09:25 kensmith Exp $

export SH="${SH:-sh}"

cd `dirname $0`

${SH} regress.sh
