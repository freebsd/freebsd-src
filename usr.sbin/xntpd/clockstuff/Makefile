#
# $Id: Makefile,v 1.4 1997/02/22 16:14:17 peter Exp $
#

PROG= propdelay
DPADD=	${LIBNTP} ${LIBM}
LDADD=	${LIBNTP} -lm

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
