#	$Id: Makefile,v 1.4 1997/02/22 16:12:49 peter Exp $

PROG =	rpc.lockd
SRCS =	nlm_prot_svc.c lockd.c procs.c
MAN8 =	rpc.lockd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CFLAGS+= -I.

CLEANFILES= nlm_prot_svc.c nlm_prot.h

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/nlm_prot.x
RPCGEN= rpcgen -L -C

nlm_prot_svc.c: ${RPCSRC} nlm_prot.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

nlm_prot.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
