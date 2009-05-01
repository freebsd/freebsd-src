#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.23.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

BINDIR?=	/bin
WARNS?=		6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
