# $FreeBSD: src/sys/modules/dtrace/dtmalloc/Makefile,v 1.1.4.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/dtmalloc

KMOD=		dtmalloc
SRCS=		dtmalloc.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
