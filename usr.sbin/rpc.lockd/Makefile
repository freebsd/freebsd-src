# $FreeBSD$

PROG =	rpc.lockd
SRCS =	nlm_prot_svc.c nlm_prot_clnt.c nlm_prot.h lockd.c procs.c
MAN=	rpc.lockd.8

MAN8 =	rpc.lockd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CFLAGS+= -I.

CLEANFILES= nlm_prot_svc.c nlm_prot.h nlm_prot_clnt.c \
			klm_prot_svc.c klm_prot.h

NLMRPCSRC= ${DESTDIR}/usr/include/rpcsvc/nlm_prot.x
KLMRPCSRC= ${DESTDIR}/usr/include/rpcsvc/klm_prot.x
RPCGEN= rpcgen -L -C

nlm_prot_clnt.c: ${NLMRPCSRC}
	${RPCGEN} -l -o ${.TARGET} ${NLMRPCSRC}

nlm_prot_svc.c: ${NLMRPCSRC}
	${RPCGEN} -m -o ${.TARGET} ${NLMRPCSRC}

nlm_prot.h: ${NLMRPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${NLMRPCSRC}

klm_prot_svc.c: ${KLMRPCSRC}
	${RPCGEN} -m -o ${.TARGET} ${KLMRPCSRC}

klm_prot.h: ${KLMRPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${KLMRPCSRC}

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
