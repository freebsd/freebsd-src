#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.5 1995/10/03 15:11:11 bde Exp $

PROG=	init
MAN8=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil -lcrypt
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.if exists(../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
DPADD=	${CRYPTOBJDIR}/libdescrypt.a ${LIBUTIL}
LDADD=	-L${CRYPTOBJDIR} -lutil -ldescrypt
BINMODE=500
.endif

.include <bsd.prog.mk>
