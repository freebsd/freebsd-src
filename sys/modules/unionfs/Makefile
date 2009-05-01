# $FreeBSD: src/sys/modules/unionfs/Makefile,v 1.12.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
