#	$Id: Makefile,v 1.9 1998/06/30 16:09:28 sos Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/procfs
KMOD=	procfs_mod
SRCS=	opt_vmpage.h procfs_ctl.c procfs_map.c procfs_note.c procfs_status.c \
	procfs_subr.c procfs_type.c procfs_vfsops.c procfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS
CLEANFILES+= opt_vmpage.h

opt_vmpage.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
