# $FreeBSD: src/tools/regression/usr.bin/sed/Makefile,v 1.4.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
