# $FreeBSD: src/sys/modules/nwfs/Makefile,v 1.11 2002/01/11 15:48:55 ru Exp $

.PATH: ${.CURDIR}/../../fs/nwfs

KMOD=	nwfs

SRCS=	vnode_if.h \
	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	nwfs_subr.c opt_ncp.h opt_nwfs.h opt_vmpage.h

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

opt_ncp.h:
	echo "#define NCP	1"  > ${.TARGET}

load: nwfs.ko
	kldload ./nwfs.ko

unload:
	@(if kldunload nwfs; then true; else true; fi)

deinstall:
	rm -f /modules/nwfs.ko

.include <bsd.kmod.mk>
