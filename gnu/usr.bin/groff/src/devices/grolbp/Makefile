# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolbp/Makefile,v 1.4.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	grolbp
SRCS=		lbp.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
