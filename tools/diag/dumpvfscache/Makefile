# $FreeBSD$

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>
