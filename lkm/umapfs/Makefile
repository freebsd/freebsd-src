#	$Id: Makefile,v 1.3 1997/02/22 12:49:25 peter Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/umapfs
KMOD=	umap_mod
SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_LKM= yes
CFLAGS+= -DUMAPFS

.include <bsd.kmod.mk>
