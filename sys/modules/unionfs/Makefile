# $FreeBSD: src/sys/modules/unionfs/Makefile,v 1.12.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
