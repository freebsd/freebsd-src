#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id$

PROG=	register
CFLAGS+=-DCRYPT -DDEBUG -DKERBEROS
DPADD=	${LIBKRB} ${LIBDES} ${LIBCRYPT}
LDADD=	-L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes -lcrypt
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
