#	From: @(#)Makefile	5.2 (Berkeley) 2/14/91
#	$Id: Makefile,v 1.3 1995/07/18 16:37:25 mark Exp $

PROG=	kdb_edit
CFLAGS+=-DKERBEROS -DDEBUG -I. -I${.CURDIR}/../include
SRCS=	kdb_edit.c maketime.c
.PATH:	${.CURDIR}/../kdb_edit
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
