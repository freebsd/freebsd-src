#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.1 1994/08/08 01:03:57 wollman Exp $

PROG=	callbootd
SRCS=	bootparam_prot_xdr.c bootparam_prot_clnt.c callbootd.c
NOMAN=	
CLEANFILES= bootparam_prot_clnt.c bootparam_prot_xdr.c  \
	    bootparam_prot.h bootparam_prot.x

bootparam_prot_clnt.c: bootparam_prot.h
	rm -f ${.CURDIR}/bootparam_prot.x
	cp /usr/include/rpcsvc/bootparam_prot.x ${.CURDIR}
	rpcgen  -l -o ${.CURDIR}/bootparam_prot_clnt.c \
		${.CURDIR}/bootparam_prot.x

bootparam_prot_xdr.c: bootparam_prot.h
	rm -f ${.CURDIR}/bootparam_prot.x
	cp /usr/include/rpcsvc/bootparam_prot.x ${.CURDIR}
	rpcgen  -c -o ${.CURDIR}/bootparam_prot_xdr.c \
		${.CURDIR}/bootparam_prot.x

bootparam_prot.h: 
	rm -f ${.CURDIR}/bootparam_prot.x
	cp /usr/include/rpcsvc/bootparam_prot.x ${.CURDIR}
	rpcgen  -h -o ${.CURDIR}/bootparam_prot.h \
		${.CURDIR}/bootparam_prot.x

.include <bsd.prog.mk>
