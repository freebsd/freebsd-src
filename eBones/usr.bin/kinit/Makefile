#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.5 1995/09/14 04:06:11 gibbs Exp $

PROG=	kinit
CFLAGS+=-DKERBEROS -DDEBUG -DBSD42
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN1=	kinit.1

.include <bsd.prog.mk>
