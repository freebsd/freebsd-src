# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.16.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/nullfs

KMOD=	nullfs
SRCS=	vnode_if.h \
	null_subr.c null_vfsops.c null_vnops.c

.include <bsd.kmod.mk>
