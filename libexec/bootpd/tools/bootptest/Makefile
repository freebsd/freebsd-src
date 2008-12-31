# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.8.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	bootptest
MAN=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
