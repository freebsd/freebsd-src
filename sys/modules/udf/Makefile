# $FreeBSD: src/sys/modules/udf/Makefile,v 1.2 2002/04/28 06:38:38 scottl Exp $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
NOMAN=

CLEANFILES=  .depend
.include <bsd.kmod.mk>
