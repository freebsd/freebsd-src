# $FreeBSD: src/sys/modules/dtrace/dtnfsclient/Makefile,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../../nfsclient

KMOD=		dtnfsclient
SRCS=		nfs_kdtrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
