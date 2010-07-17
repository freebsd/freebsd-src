# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.7.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
