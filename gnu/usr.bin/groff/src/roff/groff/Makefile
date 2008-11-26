# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.28.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
