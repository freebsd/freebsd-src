# bootptest/Makefile
# Makefile,v 1.2 1994/09/10 15:00:08 csgr Exp

PROG=	bootptest
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -I${SRCDIR}
.PATH: ${SRCDIR}
MANSRC= ${SRCDIR}

SRCS=	bootptest.c print-bootp.c getif.c getether.c report.c

MAN8=	bootptest.8

.include <bsd.prog.mk>

