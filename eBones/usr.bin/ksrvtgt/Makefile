#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.5 1995/09/14 04:06:18 gibbs Exp $

PROG=	ksrvtgt
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN1=	ksrvtgt.1

.include <bsd.prog.mk>
