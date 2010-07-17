#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/join/regress.t,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
