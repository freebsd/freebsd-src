#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.6 1996/02/13 09:12:10 joerg Exp $

PROG=	init
MAN8=	init.8
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.if exists(../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
DPADD=	${DESCRYPTOBJDIR}/libdescrypt.a ${LIBUTIL}
LDADD=	-L${DESCRYPTOBJDIR} -lutil -ldescrypt
.else
DPADD=	${SCRYPTOBJDIR}/libscrypt.a ${LIBUTIL}
LDADD=	-L${SCRYPTOBJDIR} -lutil -lscrypt
.endif

.include <bsd.prog.mk>
