#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	fortune
MAN=	fortune.6
CFLAGS+=-DDEBUG -Wall -I${.CURDIR}/../strfile
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
