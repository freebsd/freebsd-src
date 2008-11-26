#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.22.12.1 2008/10/02 02:57:24 kensmith Exp $

BINDIR?=	/bin
WARNS?=		6

.if defined(NO_DYNAMICROOT)
NO_SHARED?=	YES
.endif
