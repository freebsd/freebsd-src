# Makefile
# $Id$

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.o print-bootp.o getif.o getether.o report.o 

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
