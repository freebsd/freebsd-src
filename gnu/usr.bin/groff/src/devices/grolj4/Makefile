# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolj4/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	grolj4
MAN=		grolj4.1 lj4_font.5
SRCS=		lj4.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
