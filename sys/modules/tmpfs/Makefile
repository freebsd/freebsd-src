# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c \
	tmpfs_uma.c

.include <bsd.kmod.mk>
