# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.6 1999/08/28 00:09:25 peter Exp $

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
