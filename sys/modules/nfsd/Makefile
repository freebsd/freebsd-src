.PATH: ${SRCTOP}/sys/fs/nfsserver
KMOD=	nfsd
SRCS=	vnode_if.h \
	nfs_fha_new.c \
	nfs_nfsdserv.c \
	nfs_nfsdcache.c \
	nfs_nfsdkrpc.c \
	nfs_nfsdsocket.c \
	nfs_nfsdstate.c \
	nfs_nfsdsubs.c \
	nfs_nfsdport.c \
	opt_ufs.h \
	opt_nfs.h \
	opt_inet.h \
	opt_inet6.h \
	opt_kern_tls.h \
	opt_kgssapi.h

.include <bsd.kmod.mk>
