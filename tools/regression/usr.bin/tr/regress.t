#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/tr/regress.t,v 1.1.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
