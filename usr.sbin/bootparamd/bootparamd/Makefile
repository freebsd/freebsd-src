#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.1.1.1 1995/02/26 23:40:52 wpaul Exp $

PROG=	bootparamd
SRCS=	bootparam_prot_xdr.c bootparam_prot_svc.c bootparamd.c main.c
MAN8=	bootparamd.8
CFLAGS+= -DTFTP_DIR=\"/tftpboot\" -I.
CLEANFILES= bootparam_prot_svc.c bootparam_prot_xdr.c  \
	    bootparam_prot.h bootparam_prot.x

bootparam_prot_svc.c: bootparam_prot.h
	rm -f bootparam_prot.x
	cp ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rpcgen  -m -o bootparam_prot_svc.c bootparam_prot.x

bootparam_prot_xdr.c: bootparam_prot.h
	rm -f bootparam_prot.x
	cp ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rpcgen  -c -o bootparam_prot_xdr.c bootparam_prot.x

bootparam_prot.h: 
	rm -f bootparam_prot.x
	cp ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rpcgen  -h -o bootparam_prot.h bootparam_prot.x

.include <bsd.prog.mk>
