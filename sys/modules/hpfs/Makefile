# $FreeBSD: src/sys/modules/hpfs/Makefile,v 1.8.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/hpfs

KMOD=	hpfs
SRCS=	vnode_if.h \
	hpfs_vfsops.c hpfs_vnops.c hpfs_subr.c hpfs_lookup.c \
	hpfs_alsubr.c

.include <bsd.kmod.mk>
