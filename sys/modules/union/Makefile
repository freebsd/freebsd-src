# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/union
KMOD=	union
SRCS=	union_subr.c union_vfsops.c union_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+=	-DUNION

.include <bsd.kmod.mk>
