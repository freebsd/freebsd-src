# $FreeBSD: src/usr.sbin/rpc.statd/Makefile,v 1.14 2002/07/11 18:45:14 alfred Exp $

PROG=	rpc.statd
MAN=	rpc.statd.8
SRCS=	file.c sm_inter_svc.c sm_inter.h statd.c procs.c

CFLAGS+= -I.

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CLEANFILES= sm_inter_svc.c sm_inter.h

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/sm_inter.x
RPCGEN= rpcgen -L -C

WARNS?=	4
NO_WERROR=	1

sm_inter_svc.c: ${RPCSRC}
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

sm_inter.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
