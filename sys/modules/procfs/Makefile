# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/procfs

KMOD=	procfs
SRCS=	vnode_if.h \
	procfs_ctl.c \
	procfs_dbregs.c \
	procfs_fpregs.c \
	procfs_map.c \
	procfs_mem.c \
	procfs_note.c \
	procfs_regs.c \
	procfs_rlimit.c \
	procfs_status.c \
	procfs_subr.c \
	procfs_type.c \
	procfs_vfsops.c \
	procfs_vnops.c
NOMAN=

.include <bsd.kmod.mk>
