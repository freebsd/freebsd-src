#	From: @(#)Makefile	5.2 (Berkeley) 3/5/91
#	$Id: Makefile,v 1.2 1994/07/19 19:27:04 g89r4222 Exp $

PROG=	kstash
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes 
NOMAN=	noman

.include <bsd.prog.mk>
