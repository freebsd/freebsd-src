#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

PROG=	ext_srvtab
CFLAGS+=-DKERBEROS
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD+=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	ext_srvtab.8

.include <bsd.prog.mk>
