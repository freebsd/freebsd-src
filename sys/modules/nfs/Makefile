#       $Id: Makefile,v 1.3 1997/02/22 12:48:40 peter Exp $

.PATH:  ${.CURDIR}/../../sys/nfs
KMOD=   nfs_mod
SRCS=   nfs_bio.c nfs_node.c nfs_nqlease.c nfs_serv.c nfs_socket.c \
        nfs_srvcache.c nfs_subs.c nfs_syscalls.c nfs_vfsops.c \
	nfs_vnops.c opt_inet.h
NFS_INET?=	1	# 0/1 - requires INET to be configured in kernel
NOMAN=
VFS_LKM=

CFLAGS+= -DNFS
CLEANFILES+=	opt_inet.h

opt_inet.h:
	touch opt_inet.h
.if ${NFS_INET} > 0
	echo "#define INET 1" > opt_inet.h
.endif

.include <bsd.kmod.mk>
