#
# Makefile,v 1.2 1994/09/15 20:55:44 bde Exp
#

PROG= propdelay
LDADD+= -L${.CURDIR}/../lib -lntp -lm
DPADD+= ${.CURDIR}/../lib/libntp.a ${LIBM}

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
