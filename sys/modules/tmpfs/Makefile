# $FreeBSD: src/sys/modules/tmpfs/Makefile,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c

.include <bsd.kmod.mk>
