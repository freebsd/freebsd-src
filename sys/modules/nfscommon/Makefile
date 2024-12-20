.PATH: ${SRCTOP}/sys/fs/nfs
KMOD=	nfscommon
SRCS=	vnode_if.h \
	nfs_commonacl.c \
	nfs_commonkrpc.c \
	nfs_commonport.c \
	nfs_commonsubs.c \
	opt_inet.h \
	opt_inet6.h \
	opt_kern_tls.h \
	opt_kgssapi.h \
	opt_nfs.h \
	opt_ufs.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
