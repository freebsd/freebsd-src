# $FreeBSD: src/sys/modules/fdesc/Makefile,v 1.4.2.1 1999/08/29 16:27:10 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/fdesc
KMOD=	fdesc
SRCS=	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
