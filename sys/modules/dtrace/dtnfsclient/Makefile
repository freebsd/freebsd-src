# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/dev/dtnfsclient

KMOD=		dtnfsclient
SRCS=		dtnfsclient.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
