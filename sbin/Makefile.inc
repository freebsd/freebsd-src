#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.12 2003/11/19 16:59:00 marcel Exp $

BINDIR?=	/sbin
WARNS?=	2

.if defined(NO_DYNAMICROOT) || ${MACHINE_ARCH} == "ia64"
NOSHARED?=	YES
.endif
