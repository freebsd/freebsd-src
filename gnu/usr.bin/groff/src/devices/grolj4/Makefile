# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolj4/Makefile,v 1.4 2003/05/01 13:22:18 ru Exp $

PROG_CXX=	grolj4
SRCS=		lj4.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
