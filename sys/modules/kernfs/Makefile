# $FreeBSD: src/sys/modules/kernfs/Makefile,v 1.4.2.1 1999/08/29 16:27:23 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/kernfs
KMOD=	kernfs
SRCS=	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
