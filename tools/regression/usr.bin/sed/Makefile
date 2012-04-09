# $FreeBSD: src/tools/regression/usr.bin/sed/Makefile,v 1.4.10.2.2.1 2012/03/03 06:15:13 kensmith Exp $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
	@sh inplace_race.t
