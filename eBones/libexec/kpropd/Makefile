#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

PROG=	kpropd
CFLAGS+=-I${.CURDIR}/../../usr.sbin/kprop
DPADD+=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
