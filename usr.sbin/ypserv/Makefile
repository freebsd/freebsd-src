# $Id: Makefile,v 1.2 1995/12/16 23:01:04 bde Exp $

PROG=	ypserv
SRCS=	yp_svc.c yp_server.c yp_dblookup.c yp_dnslookup.c \
	ypxfr_clnt.c yp_main.c yp_error.c yp_access.c

MAN8=	ypserv.8

CFLAGS+= -I.

CLEANFILES= yp_svc.c ypxfr_clnt.c yp.h

RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN= rpcgen -I -C

# We need to remove the 'static' keyword from _rpcsvcstate so that
# yp_main.c can see it.
yp_svc.c: ${RPCSRC} yp.h
	rm -f ${.TARGET}
	${RPCGEN} -DYPSERV_ONLY -m ${RPCSRC} | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

ypxfr_clnt.c: ${RPCSRC} yp.h
	${RPCGEN} -DYPPUSH_ONLY -l -o ${.TARGET} ${RPCSRC}

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
