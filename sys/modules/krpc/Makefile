
.PATH: ${SRCTOP}/sys/rpc ${SRCTOP}/sys/rpc/rpcsec_tls
KMOD=	krpc
SRCS=	auth_none.c \
	auth_unix.c \
	authunix_prot.c \
	clnt_bck.c \
	clnt_dg.c \
	clnt_rc.c \
	clnt_vc.c \
	getnetconfig.c \
	rpc_callmsg.c \
	rpc_generic.c \
	rpc_prot.c \
	rpcb_clnt.c \
	rpcb_prot.c \
	replay.c \
	svc.c \
	svc_auth.c \
	svc_auth_unix.c \
	svc_dg.c \
	svc_generic.c \
	svc_vc.c

SRCS+=	rpctls_impl.c auth_tls.c

SRCS+=	opt_inet6.h opt_kern_tls.h

SRCS+=		rpctlscd.h rpctlscd_xdr.c rpctlscd_clnt.c
CLEANFILES=	rpctlscd.h rpctlscd_xdr.c rpctlscd_clnt.c

S=		${SRCTOP}/sys

rpctlscd.h:	$S/rpc/rpcsec_tls/rpctlscd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -hM $S/rpc/rpcsec_tls/rpctlscd.x | grep -v pthread.h > rpctlscd.h

rpctlscd_xdr.c:	$S/rpc/rpcsec_tls/rpctlscd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -c $S/rpc/rpcsec_tls/rpctlscd.x -o rpctlscd_xdr.c

rpctlscd_clnt.c:	$S/rpc/rpcsec_tls/rpctlscd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -lM $S/rpc/rpcsec_tls/rpctlscd.x | grep -v string.h > rpctlscd_clnt.c

SRCS+=		rpctlssd.h rpctlssd_xdr.c rpctlssd_clnt.c
CLEANFILES=	rpctlssd.h rpctlssd_xdr.c rpctlssd_clnt.c

S=		${SRCTOP}/sys

rpctlssd.h:	$S/rpc/rpcsec_tls/rpctlssd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -hM $S/rpc/rpcsec_tls/rpctlssd.x | grep -v pthread.h > rpctlssd.h

rpctlssd_xdr.c:	$S/rpc/rpcsec_tls/rpctlssd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -c $S/rpc/rpcsec_tls/rpctlssd.x -o rpctlssd_xdr.c

rpctlssd_clnt.c:	$S/rpc/rpcsec_tls/rpctlssd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -lM $S/rpc/rpcsec_tls/rpctlssd.x | grep -v string.h > rpctlssd_clnt.c

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
