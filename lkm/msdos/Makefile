#	Makefile,v 1.2 1995/03/03 18:33:04 nate Exp

.PATH:	${.CURDIR}/../../sys/msdosfs
KMOD=	msdos_mod
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DMSDOSFS

.include <bsd.kmod.mk>
