# $FreeBSD: src/sys/modules/unionfs/Makefile,v 1.12.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
