#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/13 17:24:11 markm Exp $

PROG=	klist
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
MAN1=	klist.1

.include <bsd.prog.mk>
