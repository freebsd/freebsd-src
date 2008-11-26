# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.25.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	rshd
MAN=	rshd.8

WFORMAT=0

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.include <bsd.prog.mk>
