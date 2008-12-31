#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rlogind/Makefile,v 1.21.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=	rlogind
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	2

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
