#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$FreeBSD$

PROG=	kinit
CFLAGS+=-DKERBEROS -DDEBUG -DBSD42
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN1=	kinit.1

.include <bsd.prog.mk>
