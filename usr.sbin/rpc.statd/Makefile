#	$Id: Makefile,v 1.7 1998/03/06 07:00:25 bde Exp $

PROG =	rpc.statd
SRCS =	file.c sm_inter_svc.c sm_inter.h statd.c procs.c
MAN8 =	rpc.statd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CFLAGS+= -I.

CLEANFILES= sm_inter_svc.c sm_inter.h

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/sm_inter.x
RPCGEN= rpcgen -L -C

sm_inter_svc.c: ${RPCSRC}
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

sm_inter.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
