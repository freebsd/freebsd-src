.include <src.opts.mk>

PROG=	rpc.tlsservd
MAN=	rpc.tlsservd.8
SRCS=	rpc.tlsservd.c rpc.tlscommon.c rpctlssd.h rpctlssd_svc.c rpctlssd_xdr.c

CFLAGS+= -I.

LIBADD=	ssl crypto util pthread

CLEANFILES= rpctlssd_svc.c rpctlssd_xdr.c rpctlssd.h

RPCSRC=	${SRCTOP}/sys/rpc/rpcsec_tls/rpctlssd.x
RPCGEN= RPCGEN_CPP=${CPP:Q} rpcgen -L -C -M

rpctlssd_svc.c: ${RPCSRC} rpctlssd.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

rpctlssd_xdr.c: ${RPCSRC} rpctlssd.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

rpctlssd.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.PATH:	${SRCTOP}/sys/rpc/rpcsec_tls

.include <bsd.prog.mk>
