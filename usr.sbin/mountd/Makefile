#	@(#)Makefile	8.3 (Berkeley) 1/25/94

PROG=	mountd
CFLAGS+=-DNFS -DMFS -DCD9660
MAN5=	exports.0 netgroup.0
MAN8=	mountd.0
DPADD=	${LIBRPC}
LDADD=	-lrpc

.include <bsd.prog.mk>
