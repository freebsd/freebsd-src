# $FreeBSD: src/sys/modules/nfscl/Makefile,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/nfsclient
KMOD=	nfscl
SRCS=	vnode_if.h \
	nfs_clrpcops.c \
	nfs_clkrpc.c \
	nfs_clstate.c \
	nfs_clcomsubs.c \
	nfs_clport.c \
	nfs_clbio.c \
	nfs_cllock.c \
	nfs_clnfsiod.c \
	nfs_clnode.c \
	nfs_clsubs.c \
	nfs_clvfsops.c \
	nfs_clvnops.c \
	opt_inet.h \
	opt_inet6.h \
	opt_bootp.h \
	opt_nfsroot.h \
	opt_nfs.h \
	opt_ufs.h \
	opt_kgssapi.h

.include <bsd.kmod.mk>
