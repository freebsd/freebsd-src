# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv
NOMAN=

CLEANFILES=  .depend
.include <bsd.kmod.mk>
