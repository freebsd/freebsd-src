# $FreeBSD: src/sys/modules/nwfs/Makefile,v 1.13.8.1 2006/02/10 00:38:05 yar Exp $

.PATH: ${.CURDIR}/../../fs/nwfs

KMOD=	nwfs

SRCS=	vnode_if.h \
	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	nwfs_subr.c opt_vmpage.h

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

.include <bsd.kmod.mk>
