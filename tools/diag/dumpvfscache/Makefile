# $FreeBSD$

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NOMAN=	1

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>
