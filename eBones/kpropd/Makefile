#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

PROG=	kpropd
CFLAGS+=-I${.CURDIR}/../include -I${.CURDIR}/../kprop
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
BINDIR=	/usr/libexec
NOMAN=	noman

.include <bsd.prog.mk>
