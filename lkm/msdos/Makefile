#	$Id: Makefile,v 1.4 1997/02/22 12:48:38 peter Exp $

.PATH:	${.CURDIR}/../../sys/msdosfs
KMOD=	msdos_mod
SRCS=	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c opt_diagnostic.h
NOMAN=
VFS_LKM=
CFLAGS+= -DMSDOSFS
CLEANFILES+=opt_diagnostic.h

opt_diagnostic.h:
	touch opt_diagnostic.h

.include <bsd.kmod.mk>
