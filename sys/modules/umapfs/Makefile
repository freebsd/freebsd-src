#	$Id: Makefile,v 1.3 1997/02/22 12:49:25 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/umapfs
KMOD=	umap
SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_KLD= yes
CFLAGS+= -DUMAPFS

.include <bsd.kmod.mk>
