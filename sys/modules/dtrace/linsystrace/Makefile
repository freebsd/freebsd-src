# $FreeBSD: src/sys/modules/dtrace/linsystrace/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

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
