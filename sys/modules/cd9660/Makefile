# $FreeBSD: src/sys/modules/cd9660/Makefile,v 1.16.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/cd9660

KMOD=	cd9660
SRCS=	vnode_if.h \
	cd9660_bmap.c cd9660_lookup.c cd9660_node.c cd9660_rrip.c \
	cd9660_util.c cd9660_vfsops.c cd9660_vnops.c
EXPORT_SYMS=	cd9660_iconv

.include <bsd.kmod.mk>
