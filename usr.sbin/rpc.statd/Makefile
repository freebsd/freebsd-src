#	$Id: Makefile,v 1.5 1997/02/22 16:12:51 peter Exp $

PROG =	rpc.statd
SRCS =	file.c sm_inter_svc.c statd.c procs.c
MAN8 =	rpc.statd.8
DPSRCS+= sm_inter.h

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CFLAGS+= -I.

CLEANFILES= sm_inter_svc.c sm_inter.h

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/sm_inter.x
RPCGEN= rpcgen -L -C

sm_inter_svc.c: ${RPCSRC} sm_inter.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

sm_inter.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
