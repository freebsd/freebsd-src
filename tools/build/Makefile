# $FreeBSD: src/tools/build/Makefile,v 1.12.24.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRCS=
INCSGROUPS=	INCS
INCS=		

BOOTSTRAPPING?=	0

# usr.bin/gencat needs <nl_types.h>.
.if ${BOOTSTRAPPING} < 600017
INCS+=		nl_types.h
.endif

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

.include <bsd.lib.mk>
