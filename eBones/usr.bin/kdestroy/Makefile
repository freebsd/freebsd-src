#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/13 17:24:09 markm Exp $

PROG=	kdestroy
CFLAGS+=-DKERBEROS -DDEBUG -DBSD42
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
MAN1=	kdestroy.1

.include <bsd.prog.mk>
