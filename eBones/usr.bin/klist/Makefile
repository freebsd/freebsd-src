#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1995/07/18 16:37:57 mark Exp $

PROG=	klist
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
BINDIR=	/usr/bin
NOMAN=	noman

.include <bsd.prog.mk>
