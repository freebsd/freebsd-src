# $FreeBSD: src/sys/modules/aio/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h opt_compat.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue

.include <bsd.kmod.mk>
