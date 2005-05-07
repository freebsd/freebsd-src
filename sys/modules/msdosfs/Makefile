# $FreeBSD: src/sys/modules/msdosfs/Makefile,v 1.21 2004/07/03 13:22:38 tjr Exp $

.PATH: ${.CURDIR}/../../fs/msdosfs

KMOD=	msdosfs
SRCS=	opt_msdosfs.h vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
EXPORT_SYMS=	msdosfs_iconv

opt_msdosfs.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
