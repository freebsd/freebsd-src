#	$Id: Makefile,v 1.5 1997/02/22 12:48:49 peter Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/procfs
KMOD=	procfs_mod
SRCS=   procfs_ctl.c procfs_map.c procfs_note.c procfs_status.c procfs_subr.c \
	procfs_type.c procfs_vfsops.c procfs_vnops.c opt_diagnostic.h
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS
CLEANFILES+=opt_diagnostic.h

opt_diagnostic.h:
	touch opt_diagnostic.h

.include <bsd.kmod.mk>
