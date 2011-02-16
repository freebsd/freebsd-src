# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.34.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
