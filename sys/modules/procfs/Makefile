# $FreeBSD: src/sys/modules/procfs/Makefile,v 1.18 1999/12/12 20:34:10 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/procfs
KMOD=	procfs
SRCS=	vnode_if.h \
	procfs_ctl.c procfs_map.c procfs_note.c procfs_rlimit.c \
	procfs_status.c procfs_subr.c procfs_type.c procfs_vfsops.c \
	procfs_vnops.c
NOMAN=

.include <bsd.kmod.mk>
