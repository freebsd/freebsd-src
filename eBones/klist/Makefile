#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.2 1994/07/19 19:24:37 g89r4222 Exp $

PROG=	klist
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
BINDIR=	/usr/bin
NOMAN=	noman

.include <bsd.prog.mk>
