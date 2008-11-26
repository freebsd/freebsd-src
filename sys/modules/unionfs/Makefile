# $FreeBSD: src/sys/modules/unionfs/Makefile,v 1.12.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
