#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

PROG=	kprop
CFLAGS+=-I${.CURDIR}/../include
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
BINDIR=	/usr/sbin
NOMAN=	noman

.include <bsd.prog.mk>
