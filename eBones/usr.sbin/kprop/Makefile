#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.1.1.1 1995/08/03 07:36:18 mark Exp $

PROG=	kprop
CFLAGS+=-I${.CURDIR}/../include
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
BINDIR=	/usr/sbin
NOMAN=	noman

.include <bsd.prog.mk>
