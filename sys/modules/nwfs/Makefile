#	$FreeBSD$

.PATH:	${.CURDIR}/../../nwfs

KMOD=	nwfs
KMODDEPS= ncp

SRCS=	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	opt_ncp.h opt_nwfs.h opt_vmpage.h

VFS_KLD=true

CLEANFILES= opt_ncp.h opt_nwfs.h opt_vmpage.h

NOMAN=true

CFLAGS+= -DNWFS

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

opt_ncp.h:
	echo "#define NCP	1"  > ${.TARGET}

opt_nwfs.h opt_vmpage.h:
	touch ${.TARGET}

load: nwfs.ko
	kldload ./nwfs.ko

unload:
	@(if kldunload nwfs; then true; else true; fi)

deinstall:
	rm -f /modules/nwfs.ko

.include <bsd.kmod.mk>
