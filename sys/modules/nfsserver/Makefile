# $FreeBSD$

.PATH: ${.CURDIR}/../../nfsserver ${.CURDIR}/../../nfs
KMOD=	nfsserver
SRCS=	vnode_if.h \
	nfs_serv.c nfs_srvsock.c nfs_srvcache.c nfs_srvsubs.c nfs_syscalls.c \
	nfs_common.c \
	opt_nfs.h

.include <bsd.kmod.mk>
