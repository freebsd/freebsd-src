# $Id: Makefile,v 1.5 1998/05/10 16:01:36 bde Exp $

PROG=	yppush
SRCS=	ypxfr_getmap.c yp_dblookup.c yp_error.c ypxfr_misc.c yppush_main.c \
	${GENSRCS}
GENSRCS=yp.h yp_clnt.c yppush_svc.c

MAN8=	yppush.8

CFLAGS+=-I. -I${.CURDIR}/../../libexec/ypxfr

RPCDIR=	${.CURDIR}/../../include/rpcsvc
RPCGEN= rpcgen -C

.PATH:	${RPCDIR} ${.CURDIR}/../../usr.sbin/ypserv \
	${.CURDIR}/../../libexec/ypxfr

CLEANFILES= ${GENSRCS}

yppush_svc.c: yp.x
	${RPCGEN} -DYPPUSH_ONLY -m -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: yp.x
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: yp.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

.include <bsd.prog.mk>
