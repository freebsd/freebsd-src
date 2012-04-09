# $FreeBSD: src/usr.sbin/jail/Makefile,v 1.12.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

PROG=	jail
MAN=	jail.8
DPADD=	${LIBJAIL} ${LIBUTIL}
LDADD=	-ljail -lutil

WARNS?=	6

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
