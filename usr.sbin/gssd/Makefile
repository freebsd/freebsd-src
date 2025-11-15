.include <src.opts.mk>

PACKAGE=	gssd

PROG=	gssd
MAN=	gssd.8
SRCS=	gssd.c gssd.h gssd_svc.c gssd_xdr.c gssd_prot.c

CFLAGS+= -I.
WARNS?= 1

.if ${MK_MITKRB5} != "no"
# MIT KRB5
LIBADD+=	gssapi_krb5 krb5 k5crypto krb5profile krb5support
CFLAGS+= -DMK_MITKRB5=yes
.else
# Heimdal
LIBADD+=	gssapi krb5 roken
.endif

CLEANFILES= gssd_svc.c gssd_xdr.c gssd.h

RPCSRC=	${SRCTOP}/sys/kgssapi/gssd.x
RPCGEN= RPCGEN_CPP=${CPP:Q} rpcgen -L -C -M

gssd_svc.c: ${RPCSRC} gssd.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

gssd_xdr.c: ${RPCSRC} gssd.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

gssd.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.PATH:	${SRCTOP}/sys/kgssapi

.include <bsd.prog.mk>
