#	$Id: Makefile,v 1.6 1998/02/06 23:54:26 eivind Exp $

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
