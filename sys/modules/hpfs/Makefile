# $FreeBSD: src/sys/modules/hpfs/Makefile,v 1.8.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/hpfs

KMOD=	hpfs
SRCS=	vnode_if.h \
	hpfs_vfsops.c hpfs_vnops.c hpfs_subr.c hpfs_lookup.c \
	hpfs_alsubr.c

.include <bsd.kmod.mk>
