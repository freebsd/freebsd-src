#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/logger/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

PROG=	logger
WARNS?=	6

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
