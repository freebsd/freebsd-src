# $FreeBSD: src/sys/modules/dtrace/dtnfsclient/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../nfsclient

KMOD=		dtnfsclient
SRCS=		nfs_kdtrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
