#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/13 17:24:12 markm Exp $

PROG=	ksrvtgt
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
MAN1=	ksrvtgt.1

.include <bsd.prog.mk>
