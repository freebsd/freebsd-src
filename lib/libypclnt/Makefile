# $FreeBSD$

LIB= ypclnt
SHLIB_MAJOR=1
SHLIB_MINOR=0
SRCS=	ypclnt_connect.c ypclnt_error.c ypclnt_free.c ypclnt_new.c \
	ypclnt_passwd.c
SRCS+=	${GENSRCS}
INCS=	ypclnt.h
CLEANFILES+= ${GENSRCS}
CFLAGS+= -I.
WARNS?=	4
DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

GENSRCS+= yp.h yp_clnt.c \
	yppasswd.h yppasswd_xdr.c yppasswd_clnt.c yppasswd_private.h \
	yppasswd_private_clnt.c yppasswd_private_xdr.c

RPCGEN=	rpcgen -C
RPCSRC=	${.CURDIR}/../../include/rpcsvc/yp.x
RPCSRC_PW=	${.CURDIR}/../../include/rpcsvc/yppasswd.x
RPCSRC_PRIV=	${.CURDIR}/../../usr.sbin/rpc.yppasswdd/yppasswd_private.x

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

yp_clnt.c: ${RPCSRC}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC}

yppasswd.h: ${RPCSRC_PW}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PW}

yppasswd_xdr.c: ${RPCSRC_PW}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC_PW}

yppasswd_clnt.c: ${RPCSRC_PW}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PW}

yppasswd_private.h: ${RPCSRC_PRIV}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_xdr.c: ${RPCSRC_PRIV}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_clnt.c: ${RPCSRC_PRIV}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PRIV}

.include <bsd.lib.mk>
