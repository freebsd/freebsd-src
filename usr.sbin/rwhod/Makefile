#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	rwhod
MAN=	rwhod.8

# XXX breaks on Alpha due to alignment constraints
#WARNS?=	4

DPADD=	${LIBULOG}
LDADD=	-lulog

.include <bsd.prog.mk>
