#
# $FreeBSD$
#

PROG= propdelay
LDADD+= -L${.CURDIR}/../lib -lntp -lm
DPADD+= ${.CURDIR}/../lib/libntp.a ${LIBM}

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
