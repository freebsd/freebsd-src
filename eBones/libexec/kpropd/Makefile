#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$FreeBSD$

PROG=	kpropd
CFLAGS+=-I${.CURDIR}/../../usr.sbin/kprop -DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
