#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1995/07/18 16:40:05 mark Exp $

PROG=	ksrvtgt
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include -Wall
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -ldes
BINDIR=	/usr/bin
MAN1=	ksrvtgt.1

.include <bsd.prog.mk>
