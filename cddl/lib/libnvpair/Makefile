# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/common/nvpair

LIB=	nvpair

SRCS=	libnvpair.c \
	nvpair_alloc_system.c \
	nvpair_alloc_fixed.c \
	nvpair.c

CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../include		# XXX
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
