# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/nfs 
KMOD=	nfscommon
SRCS=	vnode_if.h \
	nfs_commonacl.c \
	nfs_commonkrpc.c \
	nfs_commonport.c \
	nfs_commonsubs.c \
	opt_kgssapi.h \
	opt_ufs.h \
	opt_inet6.h \
	opt_nfs.h

.include <bsd.kmod.mk>
