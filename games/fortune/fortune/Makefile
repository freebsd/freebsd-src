#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	fortune
MAN6=	fortune.6
CFLAGS+=-DDEBUG -Wall -I${.CURDIR}/../strfile
DPADD=	${COMPAT}
LDADD=	-lcompat

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
