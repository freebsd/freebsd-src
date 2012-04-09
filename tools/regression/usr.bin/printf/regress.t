#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/printf/regress.t,v 1.1.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
