# $FreeBSD$

LIB		 = ypclnt
SHLIB_MAJOR	 = 1
SHLIB_MINOR	 = 0
SRCS		 = 
SRCS		+= ypclnt_connect.c
SRCS		+= ypclnt_error.c
SRCS		+= ypclnt_free.c
SRCS		+= ypclnt_new.c
SRCS		+= ypclnt_passwd.c
SRCS		+= ${GENSRCS}
INCS		 = ypclnt.h
CLEANFILES	+= ${GENSRCS}
CFLAGS		+= -I.
WARNS		?= 4
DPADD		+= ${LIBRPCSVC}
LDADD		+= -lrpcsvc

GENSRCS		 =
GENSRCS		+= yp.h
GENSRCS		+= yp_clnt.c
GENSRCS		+= yppasswd.h
GENSRCS		+= yppasswd_clnt.c
GENSRCS		+= yppasswd_private.h
GENSRCS		+= yppasswd_private_clnt.c
GENSRCS		+= yppasswd_private_xdr.c

RPCGEN		 = rpcgen -C
RPCSRC		 = ${.CURDIR}/../../include/rpcsvc/yp.x
RPCSRC_PW	 = ${.CURDIR}/../../include/rpcsvc/yppasswd.x
RPCSRC_PRIV	 = ${.CURDIR}/../../usr.sbin/rpc.yppasswdd/yppasswd_private.x

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

yp_clnt.c: ${RPCSRC}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC}

yppasswd.h: ${RPCSRC_PW}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PW}

yppasswd_clnt.c: ${RPCSRC_PW}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PW}

yppasswd_private.h: ${RPCSRC_PRIV}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_xdr.c: ${RPCSRC_PRIV}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_clnt.c: ${RPCSRC_PRIV}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PRIV}

.include <bsd.lib.mk>
