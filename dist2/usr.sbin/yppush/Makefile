# $Id: Makefile,v 1.6 1995/12/16 04:03:02 wpaul Exp $

PROG=	yppush
SRCS=	yp_clnt.c ypxfr_getmap.c yp_dblookup.c yppush_svc.c \
	yp_error.c ypxfr_misc.c yppush_main.c

.PATH: ${.CURDIR}/../ypserv ${.CURDIR}/../../libexec/ypxfr

MAN8=	yppush.8
CFLAGS+=-I. -I${.CURDIR}/../../libexec/ypxfr

CLEANFILES= yp.h yp_clnt.c yppush_svc.c

RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN= rpcgen -C

yppush_svc.c: ${RPCSRC} yp.h
	${RPCGEN} -DYPPUSH_ONLY -m -o ${.TARGET} ${RPCSRC}

yp_clnt.c: ${RPCSRC} yp.h
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
