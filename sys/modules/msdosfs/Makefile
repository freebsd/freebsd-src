# $FreeBSD: src/sys/modules/msdosfs/Makefile,v 1.22.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/msdosfs

KMOD=	msdosfs
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_fileno.c \
	msdosfs_lookup.c msdosfs_vfsops.c msdosfs_vnops.c
EXPORT_SYMS=	msdosfs_iconv

.include <bsd.kmod.mk>
