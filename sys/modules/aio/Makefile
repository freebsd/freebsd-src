# $FreeBSD: src/sys/modules/aio/Makefile,v 1.2.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h opt_compat.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue

.include <bsd.kmod.mk>
