#	$Id: Makefile,v 1.3 1997/02/22 12:48:36 peter Exp $

.PATH:	${.CURDIR}/../../sys/ufs/mfs
KMOD=	mfs_mod
SRCS=	mfs_vfsops.c mfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DFFS -DMFS

.include <bsd.kmod.mk>
