#	$Id: Makefile,v 1.4 1998/10/16 04:30:47 peter Exp $

.PATH:	${.CURDIR}/../../ufs/mfs
KMOD=	mfs
SRCS=	mfs_vfsops.c mfs_vnops.c opt_mfs.h
NOMAN=
VFS_KLD=
CFLAGS+= -DFFS -DMFS

opt_mfs.h:
	touch opt_mfs.h

.include <bsd.kmod.mk>
