# $FreeBSD: src/sys/modules/msdosfs/Makefile,v 1.19 2002/11/06 13:46:59 nyan Exp $

.PATH: ${.CURDIR}/../../fs/msdosfs

KMOD=	msdosfs
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c

.include <bsd.kmod.mk>
