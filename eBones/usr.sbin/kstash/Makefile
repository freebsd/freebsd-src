#	From: @(#)Makefile	5.2 (Berkeley) 3/5/91
#	$Id: Makefile,v 1.5 1995/09/13 17:24:35 markm Exp $

PROG=	kstash
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN8=	kstash.8

.include <bsd.prog.mk>
