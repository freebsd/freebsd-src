# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.7.2.1 1999/08/29 16:27:32 peter Exp $

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
