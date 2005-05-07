#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.13 2004/08/06 00:07:26 gordon Exp $

BINDIR?=	/sbin
WARNS?=	2

.if defined(NO_DYNAMICROOT)
NOSHARED?=	YES
.endif
