# $FreeBSD: src/sys/modules/tmpfs/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c

.include <bsd.kmod.mk>
