# $FreeBSD$

.PATH:	${.CURDIR}/../../msdosfs
KMOD=	msdos
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=

.include <bsd.kmod.mk>
