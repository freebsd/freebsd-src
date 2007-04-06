# $FreeBSD$

.PATH:	${.CURDIR}/../../../sys/contrib/opensolaris/common/avl

LIB=	avl
SHLIB_MAJOR?=1

SRCS=	avl.c

CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
