# $FreeBSD$

.PATH: ${.CURDIR}/../../../nfsclient

KMOD=		dtnfsclient
SRCS=		nfs_kdtrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
