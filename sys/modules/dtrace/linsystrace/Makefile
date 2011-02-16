# $FreeBSD: src/sys/modules/dtrace/linsystrace/Makefile,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

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
