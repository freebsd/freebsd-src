#	$FreeBSD: src/sys/modules/nwfs/Makefile,v 1.5 1999/12/12 20:34:08 peter Exp $

.PATH:	${.CURDIR}/../../nwfs

KMOD=	nwfs
KMODDEPS= ncp

SRCS=	vnode_if.h \
	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	nwfs_subr.c opt_ncp.h opt_nwfs.h opt_vmpage.h

NOMAN=true

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
