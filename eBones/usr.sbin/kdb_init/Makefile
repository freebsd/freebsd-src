#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1995/07/18 16:37:34 mark Exp $

PROG=	kdb_init
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include -Wall
DPADD=	${LIBKDB} ${LIBKRB}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes
MAN8=	kdb_init.8

.include <bsd.prog.mk>
