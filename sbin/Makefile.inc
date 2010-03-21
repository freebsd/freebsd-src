#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.15.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

BINDIR?=	/sbin
WARNS?=	2

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
