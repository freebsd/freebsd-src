#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rlogind
SRCS=	rlogind.c
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+= -Wall -DINET6

.include <bsd.prog.mk>
