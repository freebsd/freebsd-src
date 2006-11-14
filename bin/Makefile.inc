#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD$

BINDIR?=	/bin
WARNS?=		6

.if defined(NO_DYNAMICROOT)
NO_SHARED?=	YES
.endif
