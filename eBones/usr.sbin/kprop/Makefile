#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
# $FreeBSD$

PROG=	kprop
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
