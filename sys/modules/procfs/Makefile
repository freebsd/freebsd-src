#	$Id: Makefile,v 1.1 1994/09/21 23:27:13 wollman Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/procfs
KMOD=	procfs_mod
SRCS=	procfs_ctl.c procfs_note.c procfs_status.c procfs_subr.c \
	procfs_vfsops.c procfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS

.include <bsd.kmod.mk>
