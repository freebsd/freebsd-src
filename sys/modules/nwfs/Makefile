# $FreeBSD: src/sys/modules/nwfs/Makefile,v 1.13 2004/01/19 14:00:26 ru Exp $

.PATH: ${.CURDIR}/../../fs/nwfs

KMOD=	nwfs

SRCS=	vnode_if.h \
	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	nwfs_subr.c opt_ncp.h opt_vmpage.h

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

opt_ncp.h:
	echo "#define NCP	1"  > ${.TARGET}

.include <bsd.kmod.mk>
