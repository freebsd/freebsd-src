#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.10 1998/05/09 11:32:43 bde Exp $

PROG=	bootparamd
SRCS=	bootparamd.c main.c ${GENSRCS}
GENSRCS=bootparam_prot.h bootparam_prot_svc.c bootparam_prot_xdr.c
MAN5=	bootparams.5
MAN8=	bootparamd.8
CFLAGS+= -DTFTP_DIR=\"/tftpboot\" -I.
CLEANFILES= ${GENSRCS}

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_svc.c: ${RPCSRC}
	rpcgen -m -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC}
	rpcgen -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	rpcgen -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
