# $Id: Makefile,v 1.6 1995/12/16 04:03:02 wpaul Exp $

PROG=	ypxfr
SRCS=	ypxfr_clnt.c yp_clnt.c ypxfr_getmap.c yp_dblookup.c \
	yp_error.c ypxfr_misc.c ypxfr_main.c yp_dbwrite.c

.PATH: ${.CURDIR}/../../usr.sbin/ypserv

MAN8=	ypxfr.8
CFLAGS+=-I.

CLEANFILES= yp.h yp_clnt.c ypxfr_clnt.c

RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN= rpcgen -I -C

ypxfr_clnt.c: ${RPCSRC} yp.h
	${RPCGEN} -DYPPUSH_ONLY -l -o ${.TARGET} ${RPCSRC}

yp_clnt.c: ${RPCSRC} yp.h
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
