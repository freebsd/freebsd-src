# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lookbib/Makefile,v 1.4 2003/05/01 13:22:21 ru Exp $

PROG_CXX=	lookbib
SRCS=		lookbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
