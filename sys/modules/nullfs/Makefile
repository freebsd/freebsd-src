#	$Id: Makefile,v 1.7 1998/10/16 04:30:48 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/nullfs
KMOD=	null
SRCS=	null_subr.c null_vfsops.c null_vnops.c opt_debug_nullfs.h
NOMAN=
VFS_KLD=
CFLAGS+= -DNULLFS
CLEANFILES=	opt_debug_nullfs.h

opt_debug_nullfs.h:
		touch ${.TARGET}

.include <bsd.kmod.mk>
