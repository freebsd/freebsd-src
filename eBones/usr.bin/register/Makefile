#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.4 1995/09/13 17:24:14 markm Exp $

PROG=	register
CFLAGS+=-DCRYPT -DDEBUG -DKERBEROS
DPADD=	${LIBKRB} ${LIBDES} ${LIBCRYPT}
LDADD=	-lkrb -ldes -lcrypt
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
