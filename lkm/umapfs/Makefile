#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/miscfs/umapfs
KMOD=	umap_mod
SRCS=	umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_LKM= yes
CFLAGS+= -DUMAPFS

.include <bsd.kmod.mk>
