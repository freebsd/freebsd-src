# $FreeBSD: src/cddl/lib/libavl/Makefile,v 1.2 2007/04/16 21:20:24 ru Exp $

.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/common/avl

LIB=	avl
SRCS=	avl.c
CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
