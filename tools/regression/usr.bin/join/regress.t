#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/join/regress.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
