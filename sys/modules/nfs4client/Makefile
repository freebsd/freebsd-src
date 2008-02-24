# $FreeBSD: src/sys/modules/nfs4client/Makefile,v 1.6 2006/02/18 23:49:28 yar Exp $

.PATH: ${.CURDIR}/../../nfsclient ${.CURDIR}/../../nfs4client \
	${.CURDIR}/../../nfs ${.CURDIR}/../../rpc
KMOD=	nfs4client
SRCS=	vnode_if.h \
	nfs_bio.c nfs_lock.c nfs_node.c nfs_nfsiod.c \
	nfs_common.c \
	opt_inet.h opt_nfs.h opt_bootp.h opt_nfsroot.h \
	nfs4_dev.c nfs4_idmap.c nfs4_socket.c nfs4_subs.c \
	nfs4_vfs_subs.c  nfs4_vfsops.c nfs4_vn_subs.c nfs4_vnops.c
SRCS+=	opt_inet6.h

# USE THE RPCCLNT:
CFLAGS+= -DRPCCLNT_DEBUG
SRCS+= rpcclnt.c

# USE THE NEW IDMAPPER
CFLAGS+= -DUSE_NEW_IDMAPPER

.if !defined(KERNBUILDDIR)
NFS_INET?=	1	# 0/1 - requires INET to be configured in kernel
NFS_INET6?=	1	# 0/1 - requires INET6 to be configured in kernel

.if ${NFS_INET} > 0
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.if ${NFS_INET6} > 0
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
