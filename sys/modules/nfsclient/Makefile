#       $FreeBSD$

.PATH:  ${.CURDIR}/../../sys/nfs
KMOD=   nfs_mod
SRCS=   nfs_bio.c nfs_node.c nfs_nqlease.c nfs_serv.c nfs_socket.c \
        nfs_srvcache.c nfs_subs.c nfs_syscalls.c nfs_vfsops.c nfs_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DNFS -DINET

.include <bsd.kmod.mk>
