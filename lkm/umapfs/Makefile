#	Makefile,v 1.1 1994/09/21 23:27:14 wollman Exp

.PATH:	${.CURDIR}/../../sys/miscfs/umapfs
KMOD=	umap_mod
SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_LKM= yes
CFLAGS+= -DUMAPFS

.include <bsd.kmod.mk>
