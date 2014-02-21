# $FreeBSD$

.PATH: ${.CURDIR}/../../../fs/nfsclient

KMOD=		dtnfscl
SRCS=		nfs_clkdtrace.c \
		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>

CFLAGS+=	-include ${.CURDIR}/../../../cddl/compat/opensolaris/sys/debug_compat.h
