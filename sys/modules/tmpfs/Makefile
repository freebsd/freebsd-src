# $FreeBSD: src/sys/modules/tmpfs/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c

.include <bsd.kmod.mk>
