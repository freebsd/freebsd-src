#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	init
NOMAN=	true
DPADD=	../../lib/libcrypt/libdescrypt.a ${LIBUTIL}
LDADD=	-L../../lib/libcrypt -lutil -ldescrypt
.PATH:	${.CURDIR}/../../../sbin/init
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.include <bsd.prog.mk>
