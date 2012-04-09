# $FreeBSD: src/sys/modules/dtrace/systrace_freebsd32/Makefile,v 1.1.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/systrace

KMOD=		systrace_freebsd32
SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../.. -DFREEBSD32_SYSTRACE

.include <bsd.kmod.mk>
