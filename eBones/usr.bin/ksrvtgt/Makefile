#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/07 20:53:47 mark Exp $

PROG=	ksrvtgt
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../include -Wall
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
BINDIR=	/usr/bin
MAN1=	ksrvtgt.1

.include <bsd.prog.mk>
