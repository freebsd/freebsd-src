# $Id: Makefile,v 1.6 1995/12/16 04:03:02 wpaul Exp $

PROG=	ypxfr
SRCS=	ypxfr_clnt.c yp_clnt.c ypxfr_getmap.c yp_dblookup.c \
	yp_error.c ypxfr_misc.c ypxfr_main.c yp_dbwrite.c \
	ypxfrd_xdr.c ypxfrd_getmap.c

.PATH: ${.CURDIR}/../../usr.sbin/ypserv

MAN8=	ypxfr.8

CFLAGS+= -I.
LDADD+= -lrpcsvc

CLEANFILES= yp.h yp_clnt.c ypxfr_clnt.c

RPCDIR= ${.CURDIR}/../../include/rpcsvc
RPCGEN= rpcgen -I -C

ypxfr_clnt.c: ${RPCDIR}/yp.x yp.h
	rm -f ${.TARGET}
	${RPCGEN} -DYPPUSH_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: ${RPCDIR}/yp.x yp.h
	rm -f ${.TARGET}
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: ${RPCDIR}/yp.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

# ypxfrd_xdr.c: ${RPCDIR}/ypxfrd.x ypxfrd.h
#	rm -f ${.TARGET}
#	${RPCGEN} -c -o ${.TARGET} ${RPCDIR}/ypxfrd.x

ypxfrd.h: ${RPCDIR}/ypxfrd.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/ypxfrd.x

.include <bsd.prog.mk>
