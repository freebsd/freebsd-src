# $FreeBSD$

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
