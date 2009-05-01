# $FreeBSD: src/sys/modules/dtrace/dtrace_test/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/dtrace

KMOD=		dtrace_test
SRCS=		dtrace_test.c

SRCS+=		vnode_if.h
		
CFLAGS+=	-I${.CURDIR}/../../..

CFLAGS+=	-D_KERNEL

.include <bsd.kmod.mk>
