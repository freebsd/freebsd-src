# $FreeBSD: src/usr.sbin/gssd/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	gssd
MAN=	gssd.8
SRCS=	gssd.c gssd.h gssd_svc.c gssd_xdr.c gssd_prot.c

CFLAGS+= -I.
WARNS?= 1

DPADD=	${LIBGSSAPI}
LDADD=	-lgssapi

CLEANFILES= gssd_svc.c gssd.h

RPCSRC=	${.CURDIR}/../../sys/kgssapi/gssd.x
RPCGEN= rpcgen -L -C -M

gssd_svc.c: ${RPCSRC} gssd.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

gssd_xdr.c: ${RPCSRC} gssd.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

gssd.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

.PATH:	${.CURDIR}/../../sys/kgssapi

.include <bsd.prog.mk>
