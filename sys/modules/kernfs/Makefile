# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/kernfs
KMOD=	kernfs
SRCS=	vnode_if.h \
	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
