#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/ufs/mfs
KMOD=	mfs_mod
SRCS=	mfs_vfsops.c mfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DFFS -DMFS

.include <bsd.kmod.mk>
