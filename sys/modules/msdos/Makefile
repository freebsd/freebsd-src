# $FreeBSD$

.PATH:	${.CURDIR}/../../msdosfs
KMOD=	msdos
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DMSDOSFS

.include <bsd.kmod.mk>
