#	From: @(#)Makefile	5.2 (Berkeley) 3/5/91
#	$Id: Makefile,v 1.4 1995/09/07 20:53:56 mark Exp $

PROG=	kstash
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../include -Wall
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN8=	kstash.8
BINDIR=	/usr/sbin

.include <bsd.prog.mk>
