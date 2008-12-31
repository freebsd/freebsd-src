# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
