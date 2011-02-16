# $FreeBSD: src/usr.sbin/mtest/Makefile,v 1.6.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.include <bsd.own.mk>

PROG=	mtest
MAN=	mtest.8

BINMODE= 555
WARNS?=	2

# XXX This assumes INET support in the base system.
CFLAGS+=-DINET

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

.include <bsd.prog.mk>
