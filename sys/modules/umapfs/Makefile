# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/umapfs
KMOD=	umap
SRCS=	opt_ddb.h umap_subr.c umap_vfsops.c umap_vnops.c
NOMAN=
VFS_KLD=
CLEANFILES= opt_ddb.h

opt_ddb.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
