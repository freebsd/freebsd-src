# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lkbib/Makefile,v 1.4 2003/05/01 13:22:21 ru Exp $

PROG_CXX=	lkbib
SRCS=		lkbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
