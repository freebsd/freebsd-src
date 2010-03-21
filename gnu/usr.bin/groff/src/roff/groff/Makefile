# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.36.1 2010/02/10 00:26:20 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
