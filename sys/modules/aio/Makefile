# $FreeBSD: src/sys/modules/aio/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue

.include <bsd.kmod.mk>
