# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.34.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
