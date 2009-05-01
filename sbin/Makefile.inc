#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.15.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

BINDIR?=	/sbin
WARNS?=	2

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
