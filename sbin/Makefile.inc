#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD$

BINDIR?=	/sbin
WARNS?=	2

.if defined(NO_DYNAMICROOT) || ${MACHINE_ARCH} == "ia64"
NOSHARED?=	YES
.endif
