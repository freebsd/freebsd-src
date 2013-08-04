# $FreeBSD$

PROG=	mkimg
SRCS=	mkimg.c scheme.c
MAN=	mkimg.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
