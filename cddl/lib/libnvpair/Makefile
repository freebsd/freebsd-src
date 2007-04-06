# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
.PATH:	${.CURDIR}/../../../sys/contrib/opensolaris/common/nvpair

LIB=	nvpair
SHLIB_MAJOR?=1

SRCS=	libnvpair.c
SRCS+=	nvpair_alloc_system.c
SRCS+=	nvpair_alloc_fixed.c
SRCS+=	nvpair.c

CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../include
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
