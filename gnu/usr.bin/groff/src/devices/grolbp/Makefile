# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolbp/Makefile,v 1.4.38.1 2010/12/21 17:10:29 kensmith Exp $

PROG_CXX=	grolbp
SRCS=		lbp.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
