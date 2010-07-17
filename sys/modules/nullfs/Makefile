# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.16.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/nullfs

KMOD=	nullfs
SRCS=	vnode_if.h \
	null_subr.c null_vfsops.c null_vnops.c

.include <bsd.kmod.mk>
