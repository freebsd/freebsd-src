# $FreeBSD$

PROG=	yppush
SRCS=	yp_clnt.c ypxfr_getmap.c yp_dblookup.c yppush_svc.c \
	yp_error.c ypxfr_misc.c yppush_main.c

MAN8=	yppush.8

CFLAGS+=-I. -I${.CURDIR}/../../libexec/ypxfr

RPCDIR=	${.CURDIR}/../../include/rpcsvc
RPCGEN= rpcgen -C

.PATH:	${RPCDIR} ${.CURDIR}/../../usr.sbin/ypserv \
	${.CURDIR}/../../libexec/ypxfr

CLEANFILES= yp.h yp_clnt.c yppush_svc.c

yppush_svc.c: yp.x yp.h
	${RPCGEN} -DYPPUSH_ONLY -m -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: yp.x yp.h
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: yp.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

.include <bsd.prog.mk>
