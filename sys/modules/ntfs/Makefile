# $FreeBSD$

.PATH:	${.CURDIR}/../../ntfs
KMOD=	ntfs
SRCS=	vnode_if.h \
	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c opt_vmpage.h
NOMAN=
CFLAGS+= -DNTFS

.include <bsd.kmod.mk>
