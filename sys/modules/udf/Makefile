# $FreeBSD: src/sys/modules/udf/Makefile,v 1.5.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv

.include <bsd.kmod.mk>
