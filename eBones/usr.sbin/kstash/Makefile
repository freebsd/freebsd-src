#	From: @(#)Makefile	5.2 (Berkeley) 3/5/91
#	$Id: Makefile,v 1.3 1995/07/18 16:40:14 mark Exp $

PROG=	kstash
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include -Wall
DPADD=	${LIBKDB} ${LIBKRB}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes
MAN8=	kstash.8

.include <bsd.prog.mk>
