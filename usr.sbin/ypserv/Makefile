# $Id: Makefile,v 1.1.1.1 1995/12/16 20:54:17 wpaul Exp $

PROG=	ypserv
SRCS=	yp_svc.c yp_server.c yp_dblookup.c yp_dnslookup.c \
	yp_main.c yp_error.c yp_access.c

MAN8=	ypserv.8

CFLAGS+= -I.

CLEANFILES= yp_svc.c yp.h

RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN= rpcgen -I -C -DYPSERV_ONLY

# We need to remove the 'static' keyword from _rpcsvcstate so that
# yp_main.c can see it.
yp_svc.c: ${RPCSRC} yp.h
	rm -f ${.TARGET}
	${RPCGEN} -m ${RPCSRC} | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

yp_xdr.c: ${RPCSRC} yp.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/Makefile.yp \
		${DESTDIR}/var/yp/Makefile
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mknetid \
		${DESTDIR}/usr/libexec/mknetid

.include <bsd.prog.mk>
