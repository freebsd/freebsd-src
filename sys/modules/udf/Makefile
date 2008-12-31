# $FreeBSD: src/sys/modules/udf/Makefile,v 1.5.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv

.include <bsd.kmod.mk>
