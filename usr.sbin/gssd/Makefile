# $FreeBSD$

PROG=	gssd
MAN=	gssd.8
SRCS=	gssd.c gssd.h gssd_svc.c gssd_xdr.c gssd_prot.c

CFLAGS+= -I.
WARNS?= 1

DPADD=	${LIBGSSAPI} ${LIBKRB5} ${LIBHX509} ${LIBASN1} ${LIBROKEN} ${LIBCOM_ERR} ${LIBCRYPT} ${LIBCRYPTO}
LDADD=	-lgssapi -lkrb5 -lhx509 -lasn1 -lroken -lcom_err -lcrypt -lcrypto

CLEANFILES= gssd_svc.c gssd.h

RPCSRC=	${.CURDIR}/../../sys/kgssapi/gssd.x
RPCGEN= RPCGEN_CPP=${CPP:Q} rpcgen -L -C -M

gssd_svc.c: ${RPCSRC} gssd.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

gssd_xdr.c: ${RPCSRC} gssd.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

gssd.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.PATH:	${.CURDIR}/../../sys/kgssapi

.include <bsd.prog.mk>
