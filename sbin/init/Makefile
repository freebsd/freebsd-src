#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$id$

PROG=	init
MAN8=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil -lcrypt
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.if exists(../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DPADD=	${CRYPTOBJDIR}/libdescrypt.a ${LIBUTIL}
LDADD=	-L${CRYPTOBJDIR} -lutil -ldescrypt
BINMODE=500
.endif

.include <bsd.prog.mk>
