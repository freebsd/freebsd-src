# Makefile
# $Id: Makefile,v 1.5 1997/02/22 14:21:19 peter Exp $

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
