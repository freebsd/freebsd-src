# $FreeBSD: src/sys/modules/dtrace/dtrace_test/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/dtrace

KMOD=		dtrace_test
SRCS=		dtrace_test.c

SRCS+=		vnode_if.h
		
CFLAGS+=	-I${.CURDIR}/../../..

CFLAGS+=	-D_KERNEL

.include <bsd.kmod.mk>
