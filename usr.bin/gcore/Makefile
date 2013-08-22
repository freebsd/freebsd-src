#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	gcore
SRCS=	elfcore.c gcore.c
DPADD=	${LIBUTIL}
LDADD=	-lsbuf -lutil

WARNS?=	1

.include <bsd.prog.mk>
