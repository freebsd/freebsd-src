# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolbp/Makefile,v 1.4.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG_CXX=	grolbp
SRCS=		lbp.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
