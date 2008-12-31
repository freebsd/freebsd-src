#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/printf/regress.t,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
