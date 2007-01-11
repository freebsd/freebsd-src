#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.22 2004/12/21 09:59:40 ru Exp $

BINDIR?=	/bin
WARNS?=		6

.if defined(NO_DYNAMICROOT)
NO_SHARED?=	YES
.endif
