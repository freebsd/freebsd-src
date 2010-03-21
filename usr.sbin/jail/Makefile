# $FreeBSD: src/usr.sbin/jail/Makefile,v 1.10.10.1.4.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

PROG=	jail
MAN=	jail.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	6

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
