# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.25.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	rshd
MAN=	rshd.8

WFORMAT=0

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.include <bsd.prog.mk>
