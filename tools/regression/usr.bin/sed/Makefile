# $FreeBSD: src/tools/regression/usr.bin/sed/Makefile,v 1.4.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
