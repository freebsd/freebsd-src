#	$Id: Makefile,v 1.5 1998/02/04 21:56:37 eivind Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/nullfs
KMOD=	null_mod
SRCS=	null_subr.c null_vfsops.c null_vnops.c opt_debug_nullfs.h
NOMAN=
VFS_LKM=
CFLAGS+= -DNULLFS
CLEANFILES=	opt_debug_nullfs.h

opt_debug_nullfs.h:
		touch ${.TARGET}

.include <bsd.kmod.mk>
