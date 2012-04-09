#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: src/usr.sbin/bootparamd/callbootd/Makefile,v 1.14.22.2.2.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	callbootd
NO_MAN=
SRCS=	callbootd.c ${GENSRCS}
GENSRCS=bootparam_prot.h bootparam_prot_clnt.c bootparam_prot_xdr.c

CFLAGS+= -I.

CLEANFILES= ${GENSRCS}

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_clnt.c: ${RPCSRC}
	RPCGEN_CPP=${CPP:Q} rpcgen -C -l -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC}
	RPCGEN_CPP=${CPP:Q} rpcgen -C -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	RPCGEN_CPP=${CPP:Q} rpcgen -C -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
