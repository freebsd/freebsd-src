#	$Id: Makefile,v 1.8 1998/06/30 11:28:06 jmg Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/procfs
KMOD=	procfs_mod
SRCS=   procfs_ctl.c procfs_map.c procfs_note.c procfs_status.c procfs_subr.c \
	procfs_type.c procfs_vfsops.c procfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS
CLEANFILES+=    opt_vmpage.h

opt_vmpage.h:
	touch opt_vmpage.h

.include <bsd.kmod.mk>
