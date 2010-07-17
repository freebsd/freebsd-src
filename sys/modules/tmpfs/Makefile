# $FreeBSD: src/sys/modules/tmpfs/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c

.include <bsd.kmod.mk>
