# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolj4/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	grolj4
MAN=		grolj4.1 lj4_font.5
SRCS=		lj4.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
