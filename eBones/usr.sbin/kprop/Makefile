#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.2 1995/09/07 20:51:44 mark Exp $

PROG=	kprop
CFLAGS+=-I${.CURDIR}/../../include -Wall
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
BINDIR=	/usr/sbin
NOMAN=	noman

.include <bsd.prog.mk>
