# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lkbib/Makefile,v 1.1.2.3 2003/06/26 13:13:56 ru Exp $

PROG_CXX=	lkbib
SRCS=		lkbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
