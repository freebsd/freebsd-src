# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.32.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
