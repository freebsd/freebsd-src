# $FreeBSD: src/sys/modules/ntfs/Makefile,v 1.10 2002/01/11 15:48:55 ru Exp $

.PATH: ${.CURDIR}/../../fs/ntfs

KMOD=	ntfs
SRCS=	vnode_if.h \
	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c

.include <bsd.kmod.mk>
