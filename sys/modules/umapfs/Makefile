# $FreeBSD: src/sys/modules/umapfs/Makefile,v 1.4.2.1 1999/08/29 16:27:58 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/umapfs
KMOD=	umap
SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_KLD= yes
CFLAGS+= -DUMAPFS

.include <bsd.kmod.mk>
