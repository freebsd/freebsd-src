# $FreeBSD: src/sys/modules/ntfs/Makefile,v 1.5 1999/12/12 20:34:07 peter Exp $

.PATH:	${.CURDIR}/../../ntfs
KMOD=	ntfs
SRCS=	vnode_if.h \
	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c
NOMAN=

.include <bsd.kmod.mk>
