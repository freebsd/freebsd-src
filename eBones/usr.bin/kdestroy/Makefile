#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1995/07/18 16:37:42 mark Exp $

PROG=	kdestroy
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include -DBSD42 -Wall
DPADD=	${LIBKRB}
LDADD=	-L${KRBOBJDIR} -lkrb -ldes
BINDIR=	/usr/bin
MAN1=	kdestroy.1

.include <bsd.prog.mk>
