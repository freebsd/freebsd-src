#	$Id: Makefile,v 1.13 1998/10/16 04:30:48 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/procfs
KMOD=	procfs
SRCS=	opt_vmpage.h procfs_ctl.c procfs_map.c procfs_note.c procfs_rlimit.c \
	procfs_status.c procfs_subr.c procfs_type.c procfs_vfsops.c \
	procfs_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DPROCFS
CLEANFILES+= opt_vmpage.h

opt_vmpage.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
