#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.6 1995/09/14 04:10:15 gibbs Exp $

PROG=	kerberos
SRCS=	kerberos.c cr_err_reply.c
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
