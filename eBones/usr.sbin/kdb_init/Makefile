#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$FreeBSD$

PROG=	kdb_init
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	kdb_init.8

.include <bsd.prog.mk>
