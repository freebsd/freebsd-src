# $FreeBSD: src/sys/modules/umapfs/Makefile,v 1.8 1999/12/12 16:47:33 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/umapfs
KMOD=	umap
SRCS=	vnode_if.h \
	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=

.include <bsd.kmod.mk>
