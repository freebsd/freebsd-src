# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lookbib/Makefile,v 1.1.2.3 2003/06/26 13:13:57 ru Exp $

PROG_CXX=	lookbib
SRCS=		lookbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
