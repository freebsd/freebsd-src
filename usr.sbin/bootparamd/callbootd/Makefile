#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.2 1995/03/01 08:04:40 phk Exp $

PROG=	callbootd
SRCS=	bootparam_prot_xdr.c bootparam_prot_clnt.c callbootd.c
NOMAN=	
CFLAGS+= -I.
CLEANFILES= bootparam_prot_clnt.c bootparam_prot_xdr.c  \
	    bootparam_prot.h bootparam_prot.x

bootparam_prot_clnt.c: bootparam_prot.h
	rm -f bootparam_prot.x
	cp ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rpcgen  -l -o bootparam_prot_clnt.c bootparam_prot.x

bootparam_prot_xdr.c: bootparam_prot.h
	rm -f bootparam_prot.x
	cp ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rpcgen  -c -o bootparam_prot_xdr.c bootparam_prot.x

bootparam_prot.h: 
	rm -f bootparam_prot.x
	cp ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rpcgen  -h -o bootparam_prot.h bootparam_prot.x

.include "../../Makefile.inc"
.include <bsd.prog.mk>
