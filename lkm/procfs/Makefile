# $FreeBSD$

.PATH:	${.CURDIR}/../../sys/miscfs/procfs \
	${.CURDIR}/../../sys/${MACHINE}/${MACHINE}
KMOD=	procfs_mod
SRCS=	procfs_machdep.c procfs_ctl.c procfs_fpregs.c procfs_mem.c \
	procfs_note.c procfs_regs.c procfs_status.c procfs_subr.c \
	procfs_vfsops.c procfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPROCFS

.include <bsd.kmod.mk>
