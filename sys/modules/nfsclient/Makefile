# $FreeBSD$

.PATH: ${.CURDIR}/../../nfsclient ${.CURDIR}/../../nfs4client \
	${.CURDIR}/../../nfs ${.CURDIR}/../../rpc

KMOD=	nfsclient
SRCS=	vnode_if.h \
	nfs_bio.c nfs_lock.c nfs_node.c nfs_socket.c nfs_subs.c nfs_nfsiod.c \
	nfs_vfsops.c nfs_vnops.c nfs_common.c \
	opt_inet.h opt_nfs.h opt_bootp.h opt_nfsroot.h
SRCS+=	nfs4_dev.c nfs4_idmap.c nfs4_socket.c nfs4_subs.c \
	nfs4_vfs_subs.c  nfs4_vfsops.c nfs4_vn_subs.c nfs4_vnops.c
SRCS+=	opt_inet6.h

NFS_INET?=	1	# 0/1 - requires INET to be configured in kernel
NFS_INET6?=	1	# 0/1 - requires INET6 to be configured in kernel

# USE THE RPCCLNT:
CFLAGS+= -DRPCCLNT_DEBUG
SRCS+= rpcclnt.c

# USE THE NEW IDMAPPER
CFLAGS+= -DUSE_NEW_IDMAPPER

opt_inet.h:
	touch ${.TARGET}
.if ${NFS_INET} > 0
	echo "#define INET 1" > ${.TARGET}
.endif

.if ${NFS_INET6} > 0
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>

