#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.9 1998/04/26 16:31:14 bde Exp $

PROG=	callbootd
SRCS=	callbootd.c ${GENSRCS}
GENSRCS=bootparam_prot.h bootparam_prot_clnt.c bootparam_prot_xdr.c
NOMAN=
CFLAGS+= -I.
CLEANFILES= ${GENSRCS}

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_clnt.c: ${RPCSRC}
	rpcgen -l -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC}
	rpcgen -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	rpcgen -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
