#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD$

BINDIR?=	/sbin
WARNS?=	2

.if !defined(WITH_DYNAMICROOT)
NOSHARED?=	YES
.endif
