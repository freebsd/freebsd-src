# $FreeBSD$

.PATH:	${.CURDIR}/../../i386/linux/linprocfs
KMOD=	linprocfs
SRCS=	vnode_if.h linprocfs_misc.c linprocfs_subr.c \
		linprocfs_vfsops.c linprocfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DLINPROCFS

.include <bsd.kmod.mk>
