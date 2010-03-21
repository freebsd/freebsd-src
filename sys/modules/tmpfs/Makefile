# $FreeBSD: src/sys/modules/tmpfs/Makefile,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c

.include <bsd.kmod.mk>
