#	$Id: Makefile,v 1.6 1998/02/04 21:56:39 eivind Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/procfs
KMOD=	procfs_mod
SRCS=   procfs_ctl.c procfs_map.c procfs_note.c procfs_status.c procfs_subr.c \
	procfs_type.c procfs_vfsops.c procfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS

.include <bsd.kmod.mk>
