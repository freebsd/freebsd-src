# $FreeBSD: src/sys/modules/aio/Makefile,v 1.2 2006/10/15 14:22:13 netchild Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue

.include <bsd.kmod.mk>
