# $FreeBSD$

PROG=	rpc.ypxfrd
SRCS=	ypxfrd_svc.c ypxfrd.h ypxfrd_server.c yp_error.c \
	yp_access.c ypxfrd_main.c

RPCDIR=	${.CURDIR}/../../include/rpcsvc

.PATH:	${.CURDIR}/../../usr.sbin/ypserv

MAN=	rpc.ypxfrd.8

CFLAGS+= -I. -DXFRBLOCKSIZE=65535

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CLEANFILES= ypxfrd_svc.c ypxfrd.h

RPCGEN= rpcgen -I -C

# We need to remove the 'static' keyword from _rpcsvcstate so that
# ypxfrd_main.c can see it.
ypxfrd_svc.c: ${RPCDIR}/ypxfrd.x
	rm -f ${.TARGET}
	${RPCGEN} -m ${RPCDIR}/ypxfrd.x | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

# ypxfrd_xdr.c: ${RPCDIR}/ypxfrd.x
#	rm -f ${.TARGET}
#	${RPCGEN} -c -o ${.TARGET} ${RPCDIR}/ypxfrd.x

ypxfrd.h: ${RPCDIR}/ypxfrd.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/ypxfrd.x

.include <bsd.prog.mk>
