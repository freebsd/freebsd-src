#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.21 2004/08/06 00:07:26 gordon Exp $

BINDIR?=	/bin
WARNS?=		6

.if defined(NO_DYNAMICROOT)
NOSHARED?=	YES
.endif
