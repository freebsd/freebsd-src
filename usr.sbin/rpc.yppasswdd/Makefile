# $Id: Makefile,v 1.3 1996/02/11 02:46:48 wpaul Exp $

PROG=	rpc.yppasswdd
SRCS=	pw_copy.c pw_util.c yppasswd_svc.c yp_error.c ypxfr_misc.c \
	yp_clnt.c yp_dblookup.c yppasswd_private_xdr.c \
	util.c yppasswdd_server.c yppasswd_comm.c yppasswdd_main.c

.PATH:	${.CURDIR}/../../usr.sbin/ypserv ${.CURDIR}/../../usr.bin/chpass \
	${.CURDIR}/../../libexec/ypxfr

MAN8=	rpc.yppasswdd.8

CFLAGS+= -I. -I${.CURDIR}/../../usr.sbin/vipw \
	 -I${.CURDIR}/../../libexec/ypxfr -I${.CURDIR}/../../usr.bin/chpass \
	 -I${.CURDIR}

LDADD+=-lrpcsvc -lcrypt

CLEANFILES= yppasswd_svc.c yppasswd.h \
	    yppasswd_private_xdr.c yppasswd_private.h \
	    yp.h yp_clnt.c

RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yppasswd.x
YP_RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yp.x
PRIV_RPCSRC= ${.CURDIR}/yppasswd_private.x
RPCGEN= rpcgen -I -C

# We need to remove the 'static' keyword from _rpcsvcstate so that
# yppasswdd_main.c can see it.
yppasswd_svc.c: ${RPCSRC} yppasswd.h
	rm -f ${.TARGET}
	${RPCGEN} -m ${RPCSRC} | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

yppasswd.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

yp.h: ${YP_RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${YP_RPCSRC}

yp_clnt.c: ${YP_RPCSRC} yp.h
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${YP_RPCSRC}

yppasswd_private.h: ${PRIV_RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${PRIV_RPCSRC}

yppasswd_private_xdr.c: ${PRIV_RPCSRC} yppasswd_private.h
	${RPCGEN} -c -o ${.TARGET} ${PRIV_RPCSRC}

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/yppwupdate \
		${DESTDIR}/usr/libexec/yppwupdate

.include <bsd.prog.mk>
