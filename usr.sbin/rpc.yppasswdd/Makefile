# $Id: Makefile,v 1.2 1997/07/28 18:31:11 wpaul Exp $

PROG=	rpc.yppasswdd
SRCS=	pw_copy.c pw_util.c util.c yppasswd_svc.c yp_error.c ypxfr_misc.c \
	yp_dblookup.c yp_dbwrite.c yp_access.c yppasswd_private_xdr.c \
	yppasswd_private_svc.c yp_clnt.c yppasswdd_server.c yppasswdd_main.c

RPCDIR=	${DESTDIR}/usr/include/rpcsvc

.PATH:	${.CURDIR}/../../usr.sbin/ypserv ${.CURDIR}/../../usr.bin/chpass \
	${.CURDIR}/../../libexec/ypxfr ${RPCDIR}

MAN8=	rpc.yppasswdd.8

CFLAGS+= -I${.CURDIR}/../../usr.sbin/vipw -I${.CURDIR}/../../usr.sbin/ypserv \
	 -I${.CURDIR}/../../libexec/ypxfr -I${.CURDIR}/../../usr.bin/chpass \
	 -I${.CURDIR} -I.

DPADD=	${LIBRPCSVC} ${LIBCRYPT}
LDADD=	-lrpcsvc -lcrypt

CLEANFILES= yppasswd_svc.c yppasswd.h \
	    yppasswd_private_xdr.c yppasswd_private.h \
	    yppasswd_private_svc.c yp.h yp_clnt.c

RPCGEN= rpcgen -I -C

# We need to remove the 'static' keyword from _rpcsvcstate so that
# yppasswdd_main.c can see it.
yppasswd_svc.c: yppasswd.x yppasswd.h
	rm -f ${.TARGET}
	${RPCGEN} -m ${RPCDIR}/yppasswd.x | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

yppasswd.h: yppasswd.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yppasswd.x

yp.h: yp.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: yp.x yp.h
	rm -f ${.TARGET}
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yppasswd_private.h: yppasswd_private.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${.CURDIR}/yppasswd_private.x

yppasswd_private_xdr.c: yppasswd_private.x yppasswd_private.h
	rm -f ${.TARGET}
	${RPCGEN} -c -o ${.TARGET} ${.CURDIR}/yppasswd_private.x

yppasswd_private_svc.c: yppasswd_private.x yppasswd_private.h
	rm -f ${.TARGET}
	${RPCGEN} -m ${.CURDIR}/yppasswd_private.x | \
	sed s/"static int _rpcsvcstate = _IDLE"/"extern int _rpcsvcstate"/g > ${.TARGET}

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/yppwupdate \
		${DESTDIR}/usr/libexec/yppwupdate

.include <bsd.prog.mk>
