# $FreeBSD: src/sys/modules/fdesc/Makefile,v 1.6 1999/12/12 16:47:24 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/fdesc
KMOD=	fdesc
SRCS=	vnode_if.h \
	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
