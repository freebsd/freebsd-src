# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.8.40.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	bootptest
MAN=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
