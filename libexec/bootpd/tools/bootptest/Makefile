# Makefile
# $Id: Makefile,v 1.1.1.1 1994/09/30 05:45:07 pst Exp $

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.o print-bootp.o getif.o getether.o report.o

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
