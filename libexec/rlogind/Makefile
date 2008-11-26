#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rlogind/Makefile,v 1.20.12.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	rlogind
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+= -Wall -DINET6

.include <bsd.prog.mk>
