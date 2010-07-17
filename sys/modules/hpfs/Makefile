# $FreeBSD: src/sys/modules/hpfs/Makefile,v 1.8.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/hpfs

KMOD=	hpfs
SRCS=	vnode_if.h \
	hpfs_vfsops.c hpfs_vnops.c hpfs_subr.c hpfs_lookup.c \
	hpfs_alsubr.c

.include <bsd.kmod.mk>
