#	From: @(#)Makefile	5.2 (Berkeley) 2/14/91
#	$Id: Makefile,v 1.4 1995/09/13 17:24:24 markm Exp $

PROG=	kdb_edit
CFLAGS+=-DKERBEROS -DDEBUG -I.
SRCS=	kdb_edit.c maketime.c
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN8=	kdb_edit.8

.include <bsd.prog.mk>
