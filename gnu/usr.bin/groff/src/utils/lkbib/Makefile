# $FreeBSD: src/gnu/usr.bin/groff/src/utils/lkbib/Makefile,v 1.3 2002/04/11 11:06:06 ru Exp $

PROG_CXX=	lkbib
SRCS=		lkbib.cc
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
