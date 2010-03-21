# $FreeBSD: src/sys/modules/ntfs/Makefile,v 1.11.34.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/ntfs

KMOD=	ntfs
SRCS=	vnode_if.h \
	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c
EXPORT_SYMS=	ntfs_iconv

.include <bsd.kmod.mk>
