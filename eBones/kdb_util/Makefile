#	From: @(#)Makefile	5.2 (Berkeley) 2/14/91
#	$Id: Makefile,v 1.2 1994/07/19 19:24:09 g89r4222 Exp $

PROG=	kdb_util
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../kdb_edit \
	-I${.CURDIR}/../include
SRCS=	kdb_util.c maketime.c
.PATH:	${.CURDIR}/../kdb_edit
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
