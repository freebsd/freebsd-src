# $FreeBSD: src/cddl/lib/libnvpair/Makefile,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libnvpair
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/nvpair

LIB=	nvpair

SRCS=	libnvpair.c \
	nvpair_alloc_system.c \
	nvpair_alloc_fixed.c \
	nvpair.c

CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../include		# XXX
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
