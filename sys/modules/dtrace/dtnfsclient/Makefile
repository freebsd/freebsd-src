# $FreeBSD: src/sys/modules/dtrace/dtnfsclient/Makefile,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../nfsclient

KMOD=		dtnfsclient
SRCS=		nfs_kdtrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
