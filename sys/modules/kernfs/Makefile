# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/kernfs
KMOD=	kernfs
SRCS=	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
