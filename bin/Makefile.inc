#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.23.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

BINDIR?=	/bin
WARNS?=		6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
