#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.2 1995/09/07 20:51:49 mark Exp $

PROG=	kpropd
CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../../usr.sbin/kprop -Wall
DPADD+=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
BINDIR=	/usr/libexec
NOMAN=	noman

.include <bsd.prog.mk>
