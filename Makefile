# $FreeBSD$

PROG=	mkimg
SRCS=	mkimg.c scheme.c
MAN=	mkimg.8

BINDIR?=/usr/sbin

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	6

.include <bsd.prog.mk>
