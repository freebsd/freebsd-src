# $FreeBSD: src/sys/modules/unionfs/Makefile,v 1.12.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
