#	$Id: Makefile,v 1.4 1998/10/16 04:30:50 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/umapfs
KMOD=	umap
SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_KLD= yes
CFLAGS+= -DUMAPFS

.include <bsd.kmod.mk>
