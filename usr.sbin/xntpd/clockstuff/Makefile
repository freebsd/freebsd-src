#
# $FreeBSD: src/usr.sbin/xntpd/clockstuff/Makefile,v 1.5.2.1 1999/08/29 15:49:11 peter Exp $
#

PROG= propdelay
DPADD=	${LIBNTP} ${LIBM}
LDADD=	${LIBNTP} -lm

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
