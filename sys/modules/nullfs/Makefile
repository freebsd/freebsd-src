# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.16.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/nullfs

KMOD=	nullfs
SRCS=	vnode_if.h \
	null_subr.c null_vfsops.c null_vnops.c

.include <bsd.kmod.mk>
