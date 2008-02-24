# $FreeBSD: src/tools/regression/usr.bin/sed/Makefile,v 1.4 2007/03/14 18:58:59 dds Exp $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
