#
# $Id: Makefile,v 1.5 1998/03/07 09:46:03 bde Exp $
#

PROG= propdelay
DPADD=	${LIBNTP} ${LIBM}
LDADD=	${LIBNTP} -lm

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
