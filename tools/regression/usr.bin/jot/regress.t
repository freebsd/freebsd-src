#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/jot/regress.t,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
