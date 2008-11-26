# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.8.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	bootptest
MAN=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
