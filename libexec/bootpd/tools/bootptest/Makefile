# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootptest/Makefile,v 1.5.2.1 1999/08/29 15:03:04 peter Exp $

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
