# $FreeBSD: src/gnu/usr.bin/groff/src/roff/groff/Makefile,v 1.6 2002/10/23 21:28:52 gallatin Exp $

PROG_CXX=	groff
SRCS=		groff.cc pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
