#	$Id: Makefile,v 1.10 1998/07/01 17:02:57 bde Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/procfs
KMOD=	procfs_mod
SRCS=	opt_vmpage.h procfs_ctl.c procfs_map.c procfs_note.c procfs_status.c \
	procfs_subr.c procfs_type.c procfs_vfsops.c procfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS -DVFS_LKM_NO_DEFAULT_DISPATCH
CLEANFILES+= opt_vmpage.h

opt_vmpage.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
