#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.5 1995/09/14 04:06:04 gibbs Exp $

PROG=	kdestroy
CFLAGS+=-DKERBEROS -DDEBUG -DBSD42
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN1=	kdestroy.1

.include <bsd.prog.mk>
