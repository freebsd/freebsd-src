# $FreeBSD$

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NOMAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>
