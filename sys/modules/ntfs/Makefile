#	$Id: Makefile,v 1.1.1.1.2.1 1999/03/14 09:59:54 semenu Exp $

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
