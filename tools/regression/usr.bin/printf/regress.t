#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/printf/regress.t,v 1.1 2005/04/13 20:08:17 stefanf Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
