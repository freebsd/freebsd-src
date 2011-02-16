#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.15.14.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

BINDIR?=	/sbin
WARNS?=	2

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
