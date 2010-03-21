# $FreeBSD: src/tools/build/Makefile,v 1.12.22.1 2010/02/10 00:26:20 kensmith Exp $

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
