#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.6 1995/09/07 20:54:18 mark Exp $

PROG=	register
CFLAGS+=-DCRYPT -DDEBUG -DKERBEROS -I${.CURDIR}/../../include -Wall
DPADD=	${LIBKRB} ${LIBDES} ${LIBCRYPT}
LDADD=	-lkrb -ldes -lcrypt
BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
