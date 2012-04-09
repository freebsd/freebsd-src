# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.34.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
