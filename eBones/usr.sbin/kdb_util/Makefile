#	From: @(#)Makefile	5.2 (Berkeley) 2/14/91
#	$Id: Makefile,v 1.5 1995/09/14 04:09:56 gibbs Exp $

PROG=	kdb_util
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../kdb_edit
SRCS=	kdb_util.c maketime.c
.PATH:	${.CURDIR}/../kdb_edit
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	kdb_util.8

.include <bsd.prog.mk>
