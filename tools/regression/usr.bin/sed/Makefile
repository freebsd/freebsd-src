# $FreeBSD: src/tools/regression/usr.bin/sed/Makefile,v 1.4.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
