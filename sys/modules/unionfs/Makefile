#	$Id: Makefile,v 1.1 1994/09/16 20:24:28 wollman Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/union
KMOD=	union_mod
SRCS=	union_subr.c union_vfsops.c union_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+=	-DUNION

.include <bsd.kmod.mk>
