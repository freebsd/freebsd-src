#	$Id: Makefile,v 1.5 1998/02/04 21:56:36 eivind Exp $

.PATH:	${.CURDIR}/../../sys/msdosfs
KMOD=	msdos_mod
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DMSDOSFS

.include <bsd.kmod.mk>
