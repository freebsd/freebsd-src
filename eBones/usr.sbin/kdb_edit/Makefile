#	From: @(#)Makefile	5.2 (Berkeley) 2/14/91
#	$FreeBSD$

PROG=	kdb_edit
CFLAGS+=-DKERBEROS -DDEBUG -I.
SRCS=	kdb_edit.c maketime.c
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	kdb_edit.8

.include <bsd.prog.mk>
