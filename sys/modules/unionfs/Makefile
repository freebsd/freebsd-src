# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c
NOMAN=

.include <bsd.kmod.mk>
