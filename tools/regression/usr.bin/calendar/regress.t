#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/calendar/regress.t,v 1.1.16.1 2010/12/21 17:10:29 kensmith Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
