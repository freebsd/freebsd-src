# $FreeBSD: src/tools/regression/usr.bin/sed/Makefile,v 1.4.12.1 2010/02/10 00:26:20 kensmith Exp $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
