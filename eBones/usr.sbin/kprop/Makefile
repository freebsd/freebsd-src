#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

PROG=	kprop
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
CFLAGS+=	-DKERBEROS
NOMAN=	noman

.include <bsd.prog.mk>
