#
# $FreeBSD$
#

PROG= propdelay
DPADD=	${LIBNTP} ${LIBM}
LDADD=	${LIBNTP} -lm

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
