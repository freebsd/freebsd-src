# $FreeBSD$

.PATH: ${.CURDIR}/../../nfsclient ${.CURDIR}/../../nfs
KMOD=	nfsclient
SRCS=	vnode_if.h \
	nfs_bio.c nfs_lock.c nfs_node.c nfs_socket.c nfs_subs.c nfs_nfsiod.c \
	nfs_vfsops.c nfs_vnops.c nfs_common.c \
	opt_inet.h opt_nfs.h opt_bootp.h

NFS_INET?=	1	# 0/1 - requires INET to be configured in kernel

opt_inet.h:
	touch ${.TARGET}
.if ${NFS_INET} > 0
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
