# $FreeBSD: src/sys/modules/umapfs/Makefile,v 1.14.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/umapfs

KMOD=	umapfs
SRCS=	vnode_if.h \
	umap_subr.c umap_vfsops.c umap_vnops.c

.include <bsd.kmod.mk>
