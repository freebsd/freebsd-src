#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.5.4.1 1995/10/06 10:11:44 davidg Exp $

PROG=	bootparamd
SRCS=	bootparam_prot_xdr.c bootparam_prot_svc.c bootparamd.c main.c
MAN8=	bootparamd.8
MAN5=	bootparams.5
CFLAGS+= -DTFTP_DIR=\"/tftpboot\" -I.
CLEANFILES= bootparam_prot_svc.c bootparam_prot_xdr.c bootparam_prot.h

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_svc.c: ${RPCSRC} bootparam_prot.h
	rpcgen -m -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC} bootparam_prot.h
	rpcgen -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	rpcgen -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
