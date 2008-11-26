# $FreeBSD: src/sys/modules/fdescfs/Makefile,v 1.12.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/fdescfs

KMOD=	fdescfs
SRCS=	vnode_if.h \
	fdesc_vfsops.c fdesc_vnops.c

.include <bsd.kmod.mk>
