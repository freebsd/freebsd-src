# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.16 2002/01/11 15:48:55 ru Exp $

.PATH: ${.CURDIR}/../../fs/nullfs

KMOD=	nullfs
SRCS=	vnode_if.h \
	null_subr.c null_vfsops.c null_vnops.c

.include <bsd.kmod.mk>
