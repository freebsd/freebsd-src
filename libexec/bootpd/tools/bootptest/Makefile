# Makefile
# $Id: Makefile,v 1.2.4.1 1995/10/05 20:27:09 davidg Exp $

PROG=	bootptest
MAN8=	bootptest.8
SRCS=	bootptest.c getether.c getif.c print-bootp.c report.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
