# $FreeBSD: src/sys/modules/msdosfs/Makefile,v 1.24.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/msdosfs

KMOD=	msdosfs
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_fileno.c \
	msdosfs_lookup.c msdosfs_vfsops.c msdosfs_vnops.c
EXPORT_SYMS=	msdosfs_iconv

.include <bsd.kmod.mk>
