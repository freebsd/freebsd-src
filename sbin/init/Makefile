#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	init
MAN8=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil -lcrypt
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.include <bsd.prog.mk>
