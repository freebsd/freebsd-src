# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.8.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	bootptest
MAN=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
