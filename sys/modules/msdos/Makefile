#	$Id: Makefile,v 1.1 1994/09/21 23:27:09 wollman Exp $

.PATH:	${.CURDIR}/../../sys/msdosfs
KMOD=	msdos_mod
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DMSDOSFS

.include <bsd.kmod.mk>
