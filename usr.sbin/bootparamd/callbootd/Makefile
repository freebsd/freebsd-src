#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD$

PROG=	callbootd
NO_MAN=
SRCS=	callbootd.c ${GENSRCS}
GENSRCS=bootparam_prot.h bootparam_prot_clnt.c bootparam_prot_xdr.c

CFLAGS+= -I.

CLEANFILES= ${GENSRCS}

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_clnt.c: ${RPCSRC}
	rpcgen -C -l -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC}
	rpcgen -C -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	rpcgen -C -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
