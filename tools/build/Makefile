# $FreeBSD$

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRCS=
INCSGROUPS=	INCS
INCS=		

.if ${MACHINE_ARCH} == i386
SRCS+=		atomic.c
.endif

BOOTSTRAPPING?=	0

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

.include <bsd.lib.mk>
