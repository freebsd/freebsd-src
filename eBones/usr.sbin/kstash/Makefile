#	From: @(#)Makefile	5.2 (Berkeley) 3/5/91
#	$Id: Makefile,v 1.6 1995/09/14 04:11:14 gibbs Exp $

PROG=	kstash
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	kstash.8

.include <bsd.prog.mk>
