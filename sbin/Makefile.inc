#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.14 2004/12/21 09:59:42 ru Exp $

BINDIR?=	/sbin
WARNS?=	2

.if defined(NO_DYNAMICROOT)
NO_SHARED?=	YES
.endif
