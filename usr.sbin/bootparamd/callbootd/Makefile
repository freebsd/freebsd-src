#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.6 1995/07/04 08:39:07 bde Exp $

PROG=	callbootd
SRCS=	bootparam_prot_xdr.c bootparam_prot_clnt.c callbootd.c
NOMAN=
CFLAGS+= -I.
CLEANFILES= bootparam_prot_clnt.c bootparam_prot_xdr.c bootparam_prot.h

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_clnt.c: ${RPCSRC} bootparam_prot.h
	rpcgen -l -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC} bootparam_prot.h
	rpcgen -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	rpcgen -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
