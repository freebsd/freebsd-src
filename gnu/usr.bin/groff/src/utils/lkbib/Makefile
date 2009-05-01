# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lkbib/Makefile,v 1.4.32.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	lkbib
SRCS=		lkbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
