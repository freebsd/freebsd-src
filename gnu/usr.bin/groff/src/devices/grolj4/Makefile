# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolj4/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG_CXX=	grolj4
MAN=		grolj4.1 lj4_font.5
SRCS=		lj4.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
