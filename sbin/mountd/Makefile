#	@(#)Makefile	5.4 (Berkeley) 6/29/90

PROG=	mountd
CFLAGS+=-DNFS
MAN5=	exports.5
MAN8=	mountd.8
DPADD=	${LIBRPC}
LDADD=	-lrpc -lutil

.include <bsd.prog.mk>
