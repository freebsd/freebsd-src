# $FreeBSD: src/sys/modules/udf/Makefile,v 1.5.28.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv

.include <bsd.kmod.mk>
