#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	Makefile,v 1.5 1995/05/30 03:46:29 rgrimes Exp

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

.include <bsd.prog.mk>
