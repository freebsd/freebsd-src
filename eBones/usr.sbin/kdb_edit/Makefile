#	From: @(#)Makefile	5.2 (Berkeley) 2/14/91
#	$Id: Makefile,v 1.4 1995/09/07 20:51:11 mark Exp $

PROG=	kdb_edit
CFLAGS+=-DKERBEROS -DDEBUG -I. -I${.CURDIR}/../../include -Wall
SRCS=	kdb_edit.c maketime.c
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN8=	kdb_edit.8
BINDIR=	/usr/sbin

.include <bsd.prog.mk>
