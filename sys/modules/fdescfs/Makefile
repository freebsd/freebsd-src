# $FreeBSD: src/sys/modules/fdescfs/Makefile,v 1.12.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/fdescfs

KMOD=	fdescfs
SRCS=	vnode_if.h \
	fdesc_vfsops.c fdesc_vnops.c

.include <bsd.kmod.mk>
