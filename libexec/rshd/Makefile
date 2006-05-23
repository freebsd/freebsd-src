# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.24 2004/02/02 18:23:18 ru Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8

WFORMAT=0

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.include <bsd.prog.mk>
