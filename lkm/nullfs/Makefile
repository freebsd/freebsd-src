#	$Id: Makefile,v 1.3 1997/02/22 12:48:43 peter Exp $

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
