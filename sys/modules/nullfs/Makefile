#	$Id: Makefile,v 1.4 1998/02/01 17:34:11 bde Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/nullfs
KMOD=	null_mod
SRCS=	null_subr.c null_vfsops.c null_vnops.c \
	opt_debug_nullfs.h opt_diagnostic.h 
NOMAN=
VFS_LKM=
CFLAGS+= -DNULLFS
CLEANFILES+=opt_debug_nullfs.h opt_diagnostic.h

opt_diagnostic.h:
	touch opt_diagnostic.h

opt_debug_nullfs.h:
		touch ${.TARGET}

.include <bsd.kmod.mk>
