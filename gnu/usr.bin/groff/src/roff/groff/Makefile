# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.1.2.3 2003/06/26 13:13:56 ru Exp $

PROG_CXX=	groff
SRCS=		groff.cpp pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
