# $FreeBSD: src/sys/modules/unionfs/Makefile,v 1.12 2002/01/11 15:49:01 ru Exp $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
