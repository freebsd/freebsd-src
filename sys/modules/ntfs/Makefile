# $FreeBSD: src/sys/modules/ntfs/Makefile,v 1.1.1.1.2.2 1999/08/29 16:27:31 peter Exp $

.PATH:	${.CURDIR}/../../ntfs
KMOD=	ntfs
SRCS=	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c opt_vmpage.h
NOMAN=
VFS_KLD=
CFLAGS+= -DNTFS
CLEANFILES= opt_vmpage.h

opt_vmpage.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
