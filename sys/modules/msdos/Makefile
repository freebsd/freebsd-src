#	$Id: Makefile,v 1.6 1998/02/06 23:54:22 eivind Exp $

.PATH:	${.CURDIR}/../../msdosfs
KMOD=	msdos
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DMSDOSFS

.include <bsd.kmod.mk>
