#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	uuq
CFLAGS+=-I${.CURDIR}/../includes
BINMODE=6555
DPADD=	${LIBCOMPAT}
LDADD=	${LIBUU} -lcompat

.include <bsd.prog.mk>
