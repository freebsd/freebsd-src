# $FreeBSD: src/sys/modules/ntfs/Makefile,v 1.11.32.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/ntfs

KMOD=	ntfs
SRCS=	vnode_if.h \
	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c
EXPORT_SYMS=	ntfs_iconv

.include <bsd.kmod.mk>
