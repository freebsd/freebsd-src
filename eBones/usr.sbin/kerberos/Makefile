#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/07 20:51:30 mark Exp $

PROG=	kerberos
SRCS=	kerberos.c cr_err_reply.c
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../include -Wall
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
NOMAN=	noman
BINDIR=	/usr/sbin

.include <bsd.prog.mk>
