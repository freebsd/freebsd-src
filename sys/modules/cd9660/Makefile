# $FreeBSD: src/sys/modules/cd9660/Makefile,v 1.14 2002/01/11 15:48:37 ru Exp $

.PATH: ${.CURDIR}/../../isofs/cd9660

KMOD=	cd9660
SRCS=	vnode_if.h \
	cd9660_bmap.c cd9660_lookup.c cd9660_node.c cd9660_rrip.c \
	cd9660_util.c cd9660_vfsops.c cd9660_vnops.c
EXPORT_SYMS=	cd9660_wchar2char

.include <bsd.kmod.mk>
