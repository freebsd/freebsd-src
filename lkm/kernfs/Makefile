#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/miscfs/kernfs
KMOD=	kernfs_mod
SRCS=	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
