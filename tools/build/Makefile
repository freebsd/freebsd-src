# $FreeBSD: src/tools/build/Makefile,v 1.13.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRCS=
INCSGROUPS=	INCS
INCS=		

BOOTSTRAPPING?=	0

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

.include <bsd.lib.mk>
