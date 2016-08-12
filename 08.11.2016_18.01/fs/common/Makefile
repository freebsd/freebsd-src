#	$NetBSD: Makefile,v 1.12 2014/03/04 21:06:47 joerg Exp $
#

.include <bsd.own.mk>

LIB=	vfstest
SRCS=	fstest_ext2fs.c fstest_ffs.c fstest_lfs.c fstest_msdosfs.c	\
	fstest_nfs.c fstest_puffs.c fstest_rumpfs.c fstest_sysvbfs.c	\
	fstest_tmpfs.c fstest_udf.c fstest_v7fs.c

.if (${MKZFS} != "no")
SRCS+=	fstest_zfs.c
CPPFLAGS+=-DWANT_ZFS_TESTS
.endif

WARNS=	3

# NFSRPC libc rpc using rump syscalls
SRCS+=	bindresvport.c clnt_bcast.c clnt_dg.c clnt_generic.c clnt_vc.c \
	rpc_generic.c rpc_soc.c rpcb_clnt.c \
	svc.c svc_dg.c svc_fdset.c svc_generic.c svc_run.c svc_vc.c

LIBC=${NETBSDSRCDIR}/lib/libc
.PATH: ${NETBSDSRCDIR}/lib/libc/rpc
CPPFLAGS+=	-DPORTMAP -D_REENTRANT -DRUMP_SYS_NETWORKING
CPPFLAGS+=	-DDEBUG -DLIBWRAP -DRUMP_RPC
CPPFLAGS+=	-I${LIBC}/include -I${LIBC}/rpc


LIBISPRIVATE=	# yup

# NFS client stuff
.PATH:	${NETBSDSRCDIR}/sbin/mount_nfs ${NETBSDSRCDIR}/sbin/mount
SRCS+=	getnfsargs.c pathadj.c fattr.c mount_nfs.c

CPPFLAGS+=	-I${NETBSDSRCDIR}/sbin/mount_nfs -I${NETBSDSRCDIR}/sbin/mount
CPPFLAGS+=	-DMOUNT_NOMAIN -DMOUNTNFS_RETRYRPC=1

.include "../../../libexec/lfs_cleanerd/Makefile.inc"
CPPFLAGS+=	-DUSE_RUMP -DLFS_CLEANER_AS_LIB

CWARNFLAGS.clang+=	-Wno-error=absolute-value

.include <bsd.lib.mk>
