#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/calendar/regress.t,v 1.1 2007/06/03 03:29:32 grog Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
