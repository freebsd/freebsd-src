# $FreeBSD: src/sys/modules/msdos/Makefile,v 1.7.2.1 1999/08/29 16:27:29 peter Exp $

.PATH:	${.CURDIR}/../../msdosfs
KMOD=	msdos
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DMSDOSFS

.include <bsd.kmod.mk>
