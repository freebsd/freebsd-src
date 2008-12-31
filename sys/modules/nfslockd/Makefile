# $FreeBSD: src/sys/modules/nfslockd/Makefile,v 1.1.2.3.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../nlm ${.CURDIR}/../../rpc
KMOD=	nfslockd
SRCS=	vnode_if.h \
	nlm_advlock.c \
	nlm_prot_clnt.c \
	nlm_prot_impl.c \
	nlm_prot_server.c \
	nlm_prot_svc.c \
	nlm_prot_xdr.c \
	sm_inter_xdr.c
SRCS+=	opt_inet6.h opt_nfs.h

.if !defined(KERNBUILDDIR)
NFS_INET6?=	1	# 0/1 - requires INET6 to be configured in kernel

.if ${NFS_INET6} > 0
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
