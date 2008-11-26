#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.14.12.1 2008/10/02 02:57:24 kensmith Exp $

BINDIR?=	/sbin
WARNS?=	2

.if defined(NO_DYNAMICROOT)
NO_SHARED?=	YES
.endif
