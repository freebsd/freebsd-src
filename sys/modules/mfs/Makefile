# $FreeBSD: src/sys/modules/mfs/Makefile,v 1.5.2.1 1999/08/29 16:27:28 peter Exp $

.PATH:	${.CURDIR}/../../ufs/mfs
KMOD=	mfs
SRCS=	mfs_vfsops.c mfs_vnops.c opt_mfs.h
NOMAN=
VFS_KLD=
CLEANFILES+= opt_mfs.h

opt_mfs.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
