#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/trpt/Makefile,v 1.9.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

PROG=	trpt
MAN=	trpt.8
BINGRP=	kmem
BINMODE= 2555

WARNS?=	4

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
