# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lkbib/Makefile,v 1.4.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	lkbib
SRCS=		lkbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
