#	$Id: Makefile,v 1.1 1994/09/16 20:24:28 wollman Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/nullfs
KMOD=	null_mod
SRCS=	null_subr.c null_vfsops.c null_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DNULLFS

.include <bsd.kmod.mk>
