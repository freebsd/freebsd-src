#	$Id: Makefile,v 1.3 1997/02/22 12:48:21 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/kernfs
KMOD=	kernfs
SRCS=	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
