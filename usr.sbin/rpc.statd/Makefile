# $FreeBSD$

PROG =	rpc.statd
SRCS =	file.c sm_inter_svc.c sm_inter.h statd.c procs.c
MAN =	rpc.statd.8

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
