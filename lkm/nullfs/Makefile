#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/miscfs/nullfs
KMOD=	null_mod
SRCS=	null_subr.c null_vfsops.c null_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DNULLFS

.include <bsd.kmod.mk>
