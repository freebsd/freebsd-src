# $FreeBSD: src/sys/modules/msdos/Makefile,v 1.10.2.2 2002/12/01 09:01:08 nyan Exp $

.PATH:	${.CURDIR}/../../msdosfs
KMOD=	msdos
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=

.include <bsd.kmod.mk>
