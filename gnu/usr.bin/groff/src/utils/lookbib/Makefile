# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lookbib/Makefile,v 1.4.38.1 2010/12/21 17:10:29 kensmith Exp $

PROG_CXX=	lookbib
SRCS=		lookbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
