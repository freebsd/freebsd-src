#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.20 2003/11/19 16:59:00 marcel Exp $

BINDIR?=	/bin
WARNS?=		6

.if defined(NO_DYNAMICROOT) || ${MACHINE_ARCH} == "ia64"
NOSHARED?=	YES
.endif
