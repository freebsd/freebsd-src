# bootptest/Makefile
# $Id: Makefile,v 1.3 1994/08/22 22:19:04 gwr Exp $

PROG=	bootptest
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -I${SRCDIR}
.PATH: ${SRCDIR}
MANSRC= ${SRCDIR}

SRCS=	bootptest.c print-bootp.c getif.c getether.c report.c

MAN8=	bootptest.8

.include <bsd.prog.mk>

