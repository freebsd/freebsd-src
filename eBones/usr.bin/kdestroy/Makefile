#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/07 20:51:27 mark Exp $

PROG=	kdestroy
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../include -DBSD42 -Wall
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
MAN1=	kdestroy.1
BINDIR=	/usr/bin

.include <bsd.prog.mk>
