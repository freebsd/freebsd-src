#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	nfsd
CFLAGS+=-DNFS
MAN8=	nfsd.8
DPADD=	${LIBRPC}
LDADD=	-lrpc

.include <bsd.prog.mk>
