.include <src.opts.mk>

PROG=	rpc.tlsclntd
MAN=	rpc.tlsclntd.8
SRCS=	rpc.tlsclntd.c rpc.tlscommon.c rpctlscd.h rpctlscd_svc.c rpctlscd_xdr.c

CFLAGS+= -I. -I${SRCTOP}/usr.sbin/rpc.tlsservd

LIBADD=	ssl crypto util

CLEANFILES= rpctlscd_svc.c rpctlscd_xdr.c rpctlscd.h

RPCSRC=	${SRCTOP}/sys/rpc/rpcsec_tls/rpctlscd.x
RPCGEN= RPCGEN_CPP=${CPP:Q} rpcgen -L -C -M

rpctlscd_svc.c: ${RPCSRC} rpctlscd.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

rpctlscd_xdr.c: ${RPCSRC} rpctlscd.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

rpctlscd.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.PATH:	${SRCTOP}/sys/rpc/rpcsec_tls ${SRCTOP}/usr.sbin/rpc.tlsservd

.include <bsd.prog.mk>
