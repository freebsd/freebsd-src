# $FreeBSD$

.PATH:	${.CURDIR}/../../ufs/mfs
KMOD=	mfs
SRCS=	mfs_vfsops.c mfs_vnops.c opt_mfs.h
NOMAN=
VFS_KLD=
CLEANFILES+= opt_mfs.h

opt_mfs.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
