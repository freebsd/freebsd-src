#	$Id: Makefile,v 1.4 1998/10/16 04:30:47 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/kernfs
KMOD=	kernfs
SRCS=	kernfs_vfsops.c kernfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DKERNFS

.include <bsd.kmod.mk>
