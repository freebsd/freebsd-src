# $FreeBSD: src/sys/modules/udf/Makefile,v 1.3 2003/11/05 06:30:59 scottl Exp $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv
NOMAN=

CLEANFILES=  .depend
.include <bsd.kmod.mk>
