#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$FreeBSD$

PROG=	klist
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN1=	klist.1

.include <bsd.prog.mk>
