# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.8.34.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	bootptest
MAN=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
