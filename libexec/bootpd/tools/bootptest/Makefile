# Makefile
# $Id: Makefile,v 1.2 1995/05/30 05:45:51 rgrimes Exp $

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
