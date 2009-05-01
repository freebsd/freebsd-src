# $FreeBSD: src/sys/modules/dtrace/linsystrace/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/systrace

KMOD=		linsystrace
SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../../${MACHINE_ARCH}/linux \
		-I${.CURDIR}/../../..

CFLAGS+=	-DLINUX_SYSTRACE

.include <bsd.kmod.mk>
