# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.16.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/nullfs

KMOD=	nullfs
SRCS=	vnode_if.h \
	null_subr.c null_vfsops.c null_vnops.c

.include <bsd.kmod.mk>
