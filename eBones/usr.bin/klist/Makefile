#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/07 20:51:40 mark Exp $

PROG=	klist
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../include -Wall
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
BINDIR=	/usr/bin
MAN1=	klist.1

.include <bsd.prog.mk>
