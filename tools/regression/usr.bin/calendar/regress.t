#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/calendar/regress.t,v 1.1.14.1 2010/02/10 00:26:20 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
