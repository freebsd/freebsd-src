#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.15 2006/03/17 18:54:30 ru Exp $

.include <bsd.own.mk>

BINDIR?=	/sbin
WARNS?=	2

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
