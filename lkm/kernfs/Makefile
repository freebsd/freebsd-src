#	Makefile,v 1.1 1994/09/21 23:27:06 wollman Exp

.PATH:	${.CURDIR}/../../sys/miscfs/kernfs
KMOD=	kernfs_mod
SRCS=	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
