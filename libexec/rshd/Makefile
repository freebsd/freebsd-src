# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.23 2002/06/26 17:09:08 markm Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8

WFORMAT=0

DPADD+=	${LIBUTIL}
LDADD+= -lutil

DPADD+=	${LIBUTIL} ${LIBPAM}
LDADD+= -lutil ${MINUSLPAM}

.include <bsd.prog.mk>
