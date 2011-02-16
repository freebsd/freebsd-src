# $FreeBSD: src/sys/modules/udf/Makefile,v 1.5.34.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv

.include <bsd.kmod.mk>
