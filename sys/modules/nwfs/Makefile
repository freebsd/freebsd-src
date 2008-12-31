# $FreeBSD: src/sys/modules/nwfs/Makefile,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/nwfs

KMOD=	nwfs

SRCS=	vnode_if.h \
	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	nwfs_subr.c

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

.include <bsd.kmod.mk>
